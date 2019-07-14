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

#include "VectorMath.h"
#include "Math/Frustum.h"

namespace Math
{
    class BaseCamera
    {
    public:

        // Call this function once per frame and after you've changed any state.  This
        // regenerates all matrices.  Calling it more or less than once per frame will break
        // temporal effects and cause unpredictable results.
        virtual void Update();

        // Public functions for controlling where the camera is and its orientation
        void SetEyeAtUp( Vector3 eye, Vector3 at, Vector3 up );
        void SetLookDirection( Vector3 forward, Vector3 up );
        void SetRotation( Quaternion basisRotation );
        void SetPosition( Vector3 worldPos );
        void SetTransform( const AffineTransform& xform );
        void SetTransform( const OrthogonalTransform& xform );

        const Quaternion GetRotation() const { return m_CameraToWorld.GetRotation(); }
        const Vector3 GetRightVec() const { return m_Basis.GetX(); }
        const Vector3 GetUpVec() const { return m_Basis.GetY(); }
        // 修改该函数，原先的是反的
        const Vector3 GetForwardVec() const { return m_Basis.GetZ(); }
        const Vector3 GetPosition() const { return m_CameraToWorld.GetTranslation(); }

        // Accessors for reading the various matrices and frusta
        const Matrix4& GetViewMatrix() const { return m_ViewMatrix; }
        const Matrix4& GetProjMatrix() const { return m_ProjMatrix; }
        const Matrix4& GetViewProjMatrix() const { return m_ViewProjMatrix; }
        const Frustum& GetViewSpaceFrustum() const { return m_FrustumVS; }
        const Frustum& GetWorldSpaceFrustum() const { return m_FrustumWS; }

    protected:

        BaseCamera() : m_CameraToWorld(kIdentity), m_Basis(kIdentity) {}

        void SetProjMatrix( const Matrix4& ProjMat ) { m_ProjMatrix = ProjMat; }

        OrthogonalTransform m_CameraToWorld;

        // Redundant data cached for faster lookups.
        Matrix3 m_Basis;

        // meng
        // 0 矩阵变换
        // 1. 渲染目标从模型坐标系转到世界坐标系--->世界变换矩阵
        // 2. 再从世界坐标系转到视角坐标系--->视角变换矩阵 m_ViewMatrix
        // 3. 从视角坐标系转换到投影坐标系--->投影变换矩阵 m_ProjMatrix

        // 世界坐标系转换到视角坐标系
        Matrix4 m_ViewMatrix;        // i.e. "World-to-View" matrix

        // 视角坐标系转到投影坐标系
        Matrix4 m_ProjMatrix;        // i.e. "View-to-Projection" matrix

        // 从世界坐标系直接转换到投影坐标系
        Matrix4 m_ViewProjMatrix;    // i.e.  "World-To-Projection" matrix.

        // 视锥体剪裁
        Frustum m_FrustumVS;        // View-space view frustum
        Frustum m_FrustumWS;        // World-space view frustum
    };

    class Camera : public BaseCamera
    {
    public:
        Camera();

        virtual void Update() override;

        // Controls the view-to-projection matrix
        void SetPerspectiveMatrix( float verticalFovRadians, float aspectWidthOverHeight, float nearZClip, float farZClip );
        void SetFOV( float verticalFovInRadians ) { m_VerticalFOV = verticalFovInRadians; UpdateProjMatrix(); }
        void SetAspectRatio( float widthOverHeight) { m_AspectRatio = widthOverHeight; UpdateProjMatrix(); }
        void SetZRange( float nearZ, float farZ) { m_NearClip = nearZ; m_FarClip = farZ; UpdateProjMatrix(); }

        float GetFOV() const { return m_VerticalFOV; }
        float GetNearClip() const { return m_NearClip; }
        float GetFarClip() const { return m_FarClip; }

    private:

        void UpdateProjMatrix( void );

        float m_VerticalFOV;            // Field of view angle in radians
        float m_AspectRatio;
        float m_NearClip;
        float m_FarClip;
    };

    inline void BaseCamera::SetEyeAtUp( Vector3 eye, Vector3 at, Vector3 up )
    {
        SetLookDirection(at - eye, up);
        SetPosition(eye);
    }

    inline void BaseCamera::SetPosition( Vector3 worldPos )
    {
        m_CameraToWorld.SetTranslation( worldPos );
    }

    inline void BaseCamera::SetTransform( const AffineTransform& xform )
    {
        // By using these functions, we rederive an orthogonal transform.
        SetLookDirection(-xform.GetZ(), xform.GetY());
        SetPosition(xform.GetTranslation());
    }

    inline void BaseCamera::SetRotation( Quaternion basisRotation )
    {
        m_CameraToWorld.SetRotation(Normalize(basisRotation));
        m_Basis = Matrix3(m_CameraToWorld.GetRotation());
    }

    inline Camera::Camera()
    {
        SetPerspectiveMatrix( XM_PIDIV4, 16.0f / 9.0f, 1.0f, 1000.0f );
    }

    inline void Camera::SetPerspectiveMatrix( float verticalFovRadians, float aspectWidthOverHeight, float nearZClip, float farZClip )
    {
        m_VerticalFOV = verticalFovRadians;
        m_AspectRatio = aspectWidthOverHeight;
        m_NearClip = nearZClip;
        m_FarClip = farZClip;

        UpdateProjMatrix();
    }

} // namespace Math
