//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#include "pch.h"
#include "CommandListManager.h"

namespace Graphics
{
    extern CommandListManager g_CommandManager;
}

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE Type) :
    m_Type(Type),
    m_CommandQueue(nullptr),
    m_pFence(nullptr),
    m_NextFenceValue((uint64_t)Type << 56 | 1),
    m_LastCompletedFenceValue((uint64_t)Type << 56),
    m_AllocatorPool(Type)
{
}

CommandQueue::~CommandQueue()
{
    Shutdown();
}

void CommandQueue::Shutdown()
{
    if (m_CommandQueue == nullptr)
        return;

    m_AllocatorPool.Shutdown();

    CloseHandle(m_FenceEventHandle);

    m_pFence->Release();
    m_pFence = nullptr;

    m_CommandQueue->Release();
    m_CommandQueue = nullptr;
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
    ASSERT(pDevice != nullptr);
    ASSERT(!IsReady());
    ASSERT(m_AllocatorPool.Size() == 0);

	// 创建命令队列
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = m_Type;
    QueueDesc.NodeMask = 1;
    pDevice->CreateCommandQueue(&QueueDesc, MY_IID_PPV_ARGS(&m_CommandQueue));
    m_CommandQueue->SetName(L"CommandListManager::m_CommandQueue");

	// 创建围栏，并设置当前围栏值
    ASSERT_SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, MY_IID_PPV_ARGS(&m_pFence)));
    m_pFence->SetName(L"CommandListManager::m_pFence");
    m_pFence->Signal((uint64_t)m_Type << 56);

	// 注册围栏事件
    m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
    ASSERT(m_FenceEventHandle != INVALID_HANDLE_VALUE);

	// 创建命令分配器池
    m_AllocatorPool.Create(pDevice);

    ASSERT(IsReady());
}

uint64_t CommandQueue::ExecuteCommandList( ID3D12CommandList* List )
{
    std::lock_guard<std::mutex> LockGuard(m_FenceMutex);

    ASSERT_SUCCEEDED(((ID3D12GraphicsCommandList*)List)->Close());

	// 把list中的命令放入gpu的命令队列中
    // Kickoff the command list
    m_CommandQueue->ExecuteCommandLists(1, &List);

	// 这里相当于给gpu的命令队列中塞入一个特定的围栏值，当List执行结束，会执行这一条，并给m_pFence设置新的完成围栏值
	// 根据初始化代码可以看到，3种队列，每个的起始围栏值是不同的
	// 那这个围栏值会不会越界呢
	// 对于0号来说，他的起始围栏值=1,结束围栏值为 (1<<56)
	// 假设1秒100帧
	// 越界时间为 (1<<56)/100/3600/24/365=22849313年
	// 所以不会越界
    // Signal the next fence value (with the GPU)
    m_CommandQueue->Signal(m_pFence, m_NextFenceValue);

	// 围栏值+1
    // And increment the fence value.  
    return m_NextFenceValue++;
}

uint64_t CommandQueue::IncrementFence(void)
{
	// 增加围栏值
    std::lock_guard<std::mutex> LockGuard(m_FenceMutex);
    m_CommandQueue->Signal(m_pFence, m_NextFenceValue);
    return m_NextFenceValue++;
}

bool CommandQueue::IsFenceComplete(uint64_t FenceValue)
{
	// 判断某围栏值是否已执行
    // Avoid querying the fence value by testing against the last one seen.
    // The max() is to protect against an unlikely race condition that could cause the last
    // completed fence value to regress.
    if (FenceValue > m_LastCompletedFenceValue)
        m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_pFence->GetCompletedValue());

    return FenceValue <= m_LastCompletedFenceValue;
}

void CommandQueue::StallForFence(uint64_t FenceValue)
{
    // 等待该围栏值执行结束
    CommandQueue& Producer = Graphics::g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
    m_CommandQueue->Wait(Producer.m_pFence, FenceValue);
}

void CommandQueue::StallForProducer(CommandQueue & Producer)
{
    // 等待该命令队列执行结束
    // 注意命令队列类中仅仅存储了已经执行过的围栏和下一次的围栏。
    // 当前执行的最大围栏就是m_NextFenceValue - 1
    ASSERT(Producer.m_NextFenceValue > 0);
    m_CommandQueue->Wait(Producer.m_pFence, Producer.m_NextFenceValue - 1);
}

void CommandQueue::WaitForFence(uint64_t FenceValue)
{
    // 等待某个围栏值命令结束，期间会挂起
    if (IsFenceComplete(FenceValue))
        return;

    // TODO:  Think about how this might affect a multi-threaded situation.  Suppose thread A
    // wants to wait for fence 100, then thread B comes along and wants to wait for 99.  If
    // the fence can only have one event set on completion, then thread B has to wait for 
    // 100 before it knows 99 is ready.  Maybe insert sequential events?
    {
        std::lock_guard<std::mutex> LockGuard(m_EventMutex);

        m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
        WaitForSingleObject(m_FenceEventHandle, INFINITE);
        m_LastCompletedFenceValue = FenceValue;
    }
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
    // 申请一个该命令队列的命令分配器
    uint64_t CompletedFence = m_pFence->GetCompletedValue();

    return m_AllocatorPool.RequestAllocator(CompletedFence);
}

void CommandQueue::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator)
{
    // 需要先在命令队列中增加一个围栏值，然后执行这个函数。会标记对应的命令分配器的围栏值，可以判断是否可复用
    m_AllocatorPool.DiscardAllocator(FenceValue, Allocator);
}

CommandListManager::CommandListManager() :
    m_Device(nullptr),
    m_GraphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT),
    m_ComputeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE),
    m_CopyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{
}

CommandListManager::~CommandListManager()
{
    Shutdown();
}

void CommandListManager::Shutdown()
{
    m_GraphicsQueue.Shutdown();
    m_ComputeQueue.Shutdown();
    m_CopyQueue.Shutdown();
}

void CommandListManager::Create(ID3D12Device* pDevice)
{
    ASSERT(pDevice != nullptr);

    m_Device = pDevice;

    m_GraphicsQueue.Create(pDevice);
    m_ComputeQueue.Create(pDevice);
    m_CopyQueue.Create(pDevice);
}

void CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList * *List, ID3D12CommandAllocator * *Allocator)
{
    ASSERT(Type != D3D12_COMMAND_LIST_TYPE_BUNDLE, "Bundles are not yet supported");
    switch (Type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT: *Allocator = m_GraphicsQueue.RequestAllocator(); break;
    case D3D12_COMMAND_LIST_TYPE_BUNDLE: break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE: *Allocator = m_ComputeQueue.RequestAllocator(); break;
    case D3D12_COMMAND_LIST_TYPE_COPY: *Allocator = m_CopyQueue.RequestAllocator(); break;
    }

    ASSERT_SUCCEEDED(m_Device->CreateCommandList(1, Type, *Allocator, nullptr, MY_IID_PPV_ARGS(List)));
    (*List)->SetName(L"CommandList");
}

void CommandListManager::WaitForFence(uint64_t FenceValue)
{
    CommandQueue& Producer = Graphics::g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
    Producer.WaitForFence(FenceValue);
}
