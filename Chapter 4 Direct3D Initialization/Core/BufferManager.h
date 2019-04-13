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
#include "DepthBuffer.h"
#include "GraphicsCore.h"

namespace Graphics
{
    extern DepthBuffer g_SceneDepthBuffer;    // D32_FLOAT_S8_UINT

    void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight );
    void DestroyRenderingBuffers();

} // namespace Graphics
