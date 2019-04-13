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
#include "GraphicsCore.h"
#include "GameCore.h"
#include "SystemTime.h"
#include "DescriptorHeap.h"
#include "CommandListManager.h"

#include <dxgi1_6.h>
#include <winreg.h>        // To read the registry

#define SWAP_CHAIN_BUFFER_COUNT 3

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

using namespace Math;

namespace GameCore
{
    extern HWND g_hWnd;
}

namespace
{
    // 帧数相关
    float s_FrameTime = 0.0f;
    uint64_t s_FrameIndex = 0;
    int64_t s_FrameStartTick = 0;

    BoolVar s_LimitTo30Hz("Timing/Limit To 30Hz", false);
    BoolVar s_DropRandomFrames("Timing/Drop Random Frames", false);
}

namespace Graphics
{
    // 垂直同步
    BoolVar s_EnableVSync("Timing/VSync", true);

    uint32_t g_DisplayWidth = 1920;
    uint32_t g_DisplayHeight = 1080;

    ID3D12Device* g_Device = nullptr;

    CommandListManager g_CommandManager;

    IDXGISwapChain1* s_SwapChain1 = nullptr;

    DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
    {
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    };
}

void Graphics::Resize(uint32_t width, uint32_t height)
{
    ASSERT(s_SwapChain1 != nullptr);

    // Check for invalid window dimensions
    if (width == 0 || height == 0)
        return;

    // Check for an unneeded resize
    if (width == g_DisplayWidth && height == g_DisplayHeight)
        return;

    g_CommandManager.IdleGPU();

    g_DisplayWidth = width;
    g_DisplayHeight = height;

    DEBUGPRINT("Changing display resolution to %ux%u", width, height);

    ASSERT_SUCCEEDED(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

}

// Initialize the DirectX resources required to run.
void Graphics::Initialize(void)
{
    ASSERT(s_SwapChain1 == nullptr, "Graphics has already been initialized");

    Microsoft::WRL::ComPtr<ID3D12Device> pDevice;

#if _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    if (SUCCEEDED(D3D12GetDebugInterface(MY_IID_PPV_ARGS(&debugInterface))))
        debugInterface->EnableDebugLayer();
    else
        Utility::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
#endif

    // Obtain the DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    ASSERT_SUCCEEDED(CreateDXGIFactory2(0, MY_IID_PPV_ARGS(&dxgiFactory)));

    // Create the D3D graphics device
    Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;

    static const bool bUseWarpDriver = false;

    if (!bUseWarpDriver)
    {
        SIZE_T MaxSize = 0;

        for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (desc.DedicatedVideoMemory > MaxSize && SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, MY_IID_PPV_ARGS(&pDevice))))
            {
                pAdapter->GetDesc1(&desc);
                Utility::Printf(L"D3D12-capable hardware found:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
                MaxSize = desc.DedicatedVideoMemory;
            }
        }

        if (MaxSize > 0)
            g_Device = pDevice.Detach();
    }

    if (g_Device == nullptr)
    {
        if (bUseWarpDriver)
            Utility::Print("WARP software adapter requested.  Initializing...\n");
        else
            Utility::Print("Failed to find a hardware adapter.  Falling back to WARP.\n");
        ASSERT_SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter)));
        ASSERT_SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, MY_IID_PPV_ARGS(&pDevice)));
        g_Device = pDevice.Detach();
    }

    // 创建命令队列、命令列表、命令分配器
    g_CommandManager.Create(g_Device);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = g_DisplayWidth;
    swapChainDesc.Height = g_DisplayHeight;
    swapChainDesc.Format = SwapChainFormat;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(g_CommandManager.GetCommandQueue(), GameCore::g_hWnd, &swapChainDesc, nullptr, nullptr, &s_SwapChain1));

}

void Graphics::Terminate(void)
{
    g_CommandManager.IdleGPU();
    s_SwapChain1->SetFullscreenState(FALSE, nullptr);
}

void Graphics::Shutdown(void)
{
    g_CommandManager.Shutdown();
    s_SwapChain1->Release();
   
#if defined(_DEBUG)
    ID3D12DebugDevice * debugInterface;
    if (SUCCEEDED(g_Device->QueryInterface(&debugInterface)))
    {
        debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        debugInterface->Release();
    }
#endif

    SAFE_RELEASE(g_Device);
}

void Graphics::Present(void)
{
    UINT PresentInterval = s_EnableVSync ? std::min(4, (int)Round(s_FrameTime * 60.0f)) : 0;
    
    s_SwapChain1->Present(PresentInterval, 0);

    // Test robustness to handle spikes in CPU time
    //if (s_DropRandomFrames)
    //{
    //    if (std::rand() % 25 == 0)
    //        BusyLoopSleep(0.010);
    //}

    int64_t CurrentTick = SystemTime::GetCurrentTick();

    if (s_EnableVSync)
    {
        // With VSync enabled, the time step between frames becomes a multiple of 16.666 ms.  We need
        // to add logic to vary between 1 and 2 (or 3 fields).  This delta time also determines how
        // long the previous frame should be displayed (i.e. the present interval.)
        s_FrameTime = (s_LimitTo30Hz ? 2.0f : 1.0f) / 60.0f;
        if (s_DropRandomFrames)
        {
            if (std::rand() % 50 == 0)
                s_FrameTime += (1.0f / 60.0f);
        }
    }
    else
    {
        // When running free, keep the most recent total frame time as the time step for
        // the next frame simulation.  This is not super-accurate, but assuming a frame
        // time varies smoothly, it should be close enough.
        s_FrameTime = (float)SystemTime::TimeBetweenTicks(s_FrameStartTick, CurrentTick);
    }

    s_FrameStartTick = CurrentTick;

    ++s_FrameIndex;
}