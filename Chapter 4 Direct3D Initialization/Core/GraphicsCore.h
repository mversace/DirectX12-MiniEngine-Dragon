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

#pragma once

#include "DescriptorHeap.h"

class CommandListManager;

namespace Graphics
{
#ifndef RELEASE
    extern const GUID WKPDID_D3DDebugObjectName;
#endif

    using namespace Microsoft::WRL;

    void Initialize(void);
    void Resize(uint32_t width, uint32_t height);
    void Terminate(void);
    void Shutdown(void);
    void Present(void);

    extern uint32_t g_DisplayWidth;
    extern uint32_t g_DisplayHeight;

    extern ID3D12Device* g_Device;
    extern CommandListManager g_CommandManager;

    extern DescriptorAllocator g_DescriptorAllocator[];
    inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
    {
        return g_DescriptorAllocator[Type].Allocate(Count);
    }
}
