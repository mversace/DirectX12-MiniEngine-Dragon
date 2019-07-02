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
#include "Camera.h"
#include <cmath>

using namespace Math;

// meng 整体修改为左手坐标系
void BaseCamera::SetLookDirection( Vector3 forward, Vector3 up )
{
    // 计算前方
    Scalar forwardLenSq = LengthSquare(forward);
    forward = Select(forward * RecipSqrt(forwardLenSq), Vector3(kZUnitVector), forwardLenSq < Scalar(0.000001f));

    // 根据提供的上和前方，计算右方
    Vector3 right = Cross(up, forward);
    Scalar rightLenSq = LengthSquare(right);
    right = Select(right * RecipSqrt(rightLenSq), Cross(Vector3(kYUnitVector), forward), rightLenSq < Scalar(0.000001f));

    // 正交化，计算实际的上方
    up = Cross(forward, right);

    // 计算摄像机的转换矩阵
    m_Basis = Matrix3(right, up, forward);
    m_CameraToWorld.SetRotation(Quaternion(m_Basis));
}

void BaseCamera::Update()
{
    // 计算视角变换矩阵，还没有看懂 m_CameraToWorld
    m_ViewMatrix = Matrix4(~m_CameraToWorld);
    
    // Matrix4中的*重载，故意反着写的。所以这里反着乘
    // 计算视角投影转换矩阵。这样拿到世界矩阵再乘以这个值就可以算出最终的投影坐标了
    m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
}

void Camera::Update()
{
    BaseCamera::Update();

    m_FrustumVS = Frustum(m_ProjMatrix, m_NearClip / m_FarClip);
    m_FrustumWS = m_CameraToWorld * m_FrustumVS;
}

void Camera::UpdateProjMatrix( void )
{
    DirectX::XMMATRIX mat = XMMatrixPerspectiveFovLH(m_VerticalFOV, m_AspectRatio, m_NearClip, m_FarClip);

    SetProjMatrix(Matrix4(mat));
}