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
#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "EsramAllocator.h"

namespace Graphics
{
    DepthBuffer g_SceneDepthBuffer;
}

#define T2X_COLOR_FORMAT DXGI_FORMAT_R10G10B10A2_UNORM
#define HDR_MOTION_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT

void Graphics::InitializeRenderingBuffers( uint32_t bufferWidth, uint32_t bufferHeight )
{
    g_SceneDepthBuffer.Create( L"Scene Depth Buffer", bufferWidth, bufferHeight, DSV_FORMAT );
}
void Graphics::DestroyRenderingBuffers()
{
    g_SceneDepthBuffer.Destroy();
}
