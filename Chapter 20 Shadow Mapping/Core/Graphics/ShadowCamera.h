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

#include "Camera.h"

namespace GameCore
{
    using namespace Math;

    class ShadowCamera : public BaseCamera
    {
    public:

        ShadowCamera() {}

        void UpdateMatrix( 
            Vector3 LightDirection,     // 光照方向（单位向量）
            Vector3 ShadowCenter,       // 阴影目标的中心点
            Vector3 ShadowBounds,       // 阴影椎体的宽高深
            uint32_t BufferWidth,       // Shadow buffer width
            uint32_t BufferHeight,      // Shadow buffer height--usually same as width
            uint32_t BufferPrecision    // Bit depth of shadow buffer--usually 16 or 24
            );

        // Used to transform world space to texture space for shadow sampling
        const Matrix4& GetShadowMatrix() const { return m_ShadowMatrix; }

    private:

        Matrix4 m_ShadowMatrix;
    };

}
