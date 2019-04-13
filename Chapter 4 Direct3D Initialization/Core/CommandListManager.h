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

/*
	CommandQueue：命令队列类
	CommandListManager: 命令列表管理器

	详情见：readme.txt
*/

#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <stdint.h>
#include "CommandAllocatorPool.h"

class CommandQueue
{
    friend class CommandListManager;
    friend class CommandContext;

public:
    CommandQueue(D3D12_COMMAND_LIST_TYPE Type);
    ~CommandQueue();

	// 创建命令队列
    void Create(ID3D12Device* pDevice);
    void Shutdown();

    inline bool IsReady()
    {
        return m_CommandQueue != nullptr;
    }

    uint64_t IncrementFence(void);

	// 该围栏是否结束
    bool IsFenceComplete(uint64_t FenceValue);
	// 等待围栏结束，需要确保此时该围栏必定还没有结束
    void StallForFence(uint64_t FenceValue);
	// 等待该命令队列全部内容结束，需要确保此时该命令队列必定还没有结束
    void StallForProducer(CommandQueue& Producer);
	// 等待围栏结束
    void WaitForFence(uint64_t FenceValue);
	// 向命令队列中添加一个围栏值，等待结束
    void WaitForIdle(void) { WaitForFence(IncrementFence()); }

    ID3D12CommandQueue* GetCommandQueue() { return m_CommandQueue; }

    uint64_t GetNextFenceValue() { return m_NextFenceValue; }

private:
	// 把命令列表的内容插入gpu的命令队列
    uint64_t ExecuteCommandList(ID3D12CommandList* List);
	// 申请一个可用的该命令队列的命令分配器
    ID3D12CommandAllocator* RequestAllocator(void);
	// 当一个命令队列执行了ExecuteCommandList后，插入一个围栏值，并调用本函数记录命令分配器的key
    void DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator);

    ID3D12CommandQueue* m_CommandQueue; // 命令队列

    const D3D12_COMMAND_LIST_TYPE m_Type; // 该命令队列类型

    CommandAllocatorPool m_AllocatorPool; // 命令分配器池，类型m_Type
    std::mutex m_FenceMutex;
    std::mutex m_EventMutex;

    // Lifetime of these objects is managed by the descriptor cache
    ID3D12Fence* m_pFence;	// 围栏，可以获取已经执行完的围栏值，来判断当前命令队列对应的命令分配器是否可以复用
    uint64_t m_NextFenceValue;
    uint64_t m_LastCompletedFenceValue;
    HANDLE m_FenceEventHandle;

};

class CommandListManager
{
    friend class CommandContext;

public:
    CommandListManager();
    ~CommandListManager();

    void Create(ID3D12Device* pDevice);
    void Shutdown();

    CommandQueue& GetGraphicsQueue(void) { return m_GraphicsQueue; }
    CommandQueue& GetComputeQueue(void) { return m_ComputeQueue; }
    CommandQueue& GetCopyQueue(void) { return m_CopyQueue; }

    CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        switch (Type)
        {
        case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_ComputeQueue;
        case D3D12_COMMAND_LIST_TYPE_COPY: return m_CopyQueue;
        default: return m_GraphicsQueue;
        }
    }

    ID3D12CommandQueue* GetCommandQueue()
    {
        return m_GraphicsQueue.GetCommandQueue();
    }

	// 根据类型创建一个命令列表以及对应的命令分配器
    void CreateNewCommandList(
        D3D12_COMMAND_LIST_TYPE Type,
        ID3D12GraphicsCommandList** List,
        ID3D12CommandAllocator** Allocator);

    // Test to see if a fence has already been reached
    bool IsFenceComplete(uint64_t FenceValue)
    {
        return GetQueue(D3D12_COMMAND_LIST_TYPE(FenceValue >> 56)).IsFenceComplete(FenceValue);
    }

    // The CPU will wait for a fence to reach a specified value
    void WaitForFence(uint64_t FenceValue);

    // The CPU will wait for all command queues to empty (so that the GPU is idle)
    void IdleGPU(void)
    {
        m_GraphicsQueue.WaitForIdle();
        m_ComputeQueue.WaitForIdle();
        m_CopyQueue.WaitForIdle();
    }

private:

    ID3D12Device* m_Device;

	// 围护3个命令队列
    CommandQueue m_GraphicsQueue;
    CommandQueue m_ComputeQueue;
    CommandQueue m_CopyQueue;
};
