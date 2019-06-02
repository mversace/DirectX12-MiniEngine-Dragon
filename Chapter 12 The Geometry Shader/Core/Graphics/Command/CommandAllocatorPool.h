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
	命令分配器池。
	用于管理所有命令分配器。

	详情见：readme.txt
*/

#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <stdint.h>

class CommandAllocatorPool
{
public:
	// 该命令分配器池的类型
    CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE Type);
    ~CommandAllocatorPool();

	// 初始化命令分配器池
    void Create(ID3D12Device* pDevice);
	// 关闭命令分配器池
    void Shutdown();

	// 根据当前已经执行完的围栏值，获取一个分配器
    ID3D12CommandAllocator* RequestAllocator(uint64_t CompletedFenceValue);
	// 当该分配器对应的命令列表已经被ExecuteCommandLists后调用，需要填入当前命令列表的围栏值
    void DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator);

    inline size_t Size() { return m_AllocatorPool.size(); }

private:
    const D3D12_COMMAND_LIST_TYPE m_cCommandListType;

    ID3D12Device* m_Device;
    std::vector<ID3D12CommandAllocator*> m_AllocatorPool;
    std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_ReadyAllocators;
    std::mutex m_AllocatorMutex;
};
