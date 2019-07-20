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

#include "ColorBuffer.h"
#include "ColorCubeBuffer.h"
#include "DepthBuffer.h"
#include "ShadowBuffer.h"
#include "GpuBuffer.h"
#include "GraphicsCore.h"

namespace Graphics
{
    extern DepthBuffer g_SceneDepthBuffer;    // D32_FLOAT_S8_UINT
    extern ColorBuffer g_SceneColorBuffer;    // R11G11B10_FLOAT
    extern ColorBuffer g_OverlayBuffer;        // R8G8B8A8_UNORM
    extern ColorBuffer g_HorizontalBuffer;    // For separable (bicubic) upsampling
    extern ColorCubeBuffer g_SceneCubeBuff;     // Ìì¿ÕºÐ6¸öRTVµÄcolorbuffer

    extern ShadowBuffer g_ShadowBuffer;

    void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight );
    void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
    void DestroyRenderingBuffers();

} // namespace Graphics
