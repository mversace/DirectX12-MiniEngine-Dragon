#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"

class RootSignature;
class StructuredBuffer;
class ByteAddressBuffer;
class GraphicsPSO;
using namespace Math;
using namespace GameCore;
class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:
    // 根签名
    RootSignature m_RootSignature;
    // 顶点缓冲区
    StructuredBuffer m_VertexBuffer;
    // 索引缓冲区
    ByteAddressBuffer m_IndexBuffer;
    // 流水线对象
    GraphicsPSO m_PSO;

    Camera m_Camera;
    Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    // 半径
    float m_radius = 5.0f;
    // x方向弧度
    float m_xRotate = 0.0f;
    float m_xLast = 0.0f;
    float m_xDiff = 0.0f;
    // y方向弧度
    float m_yRotate = 0.0f;
    float m_yLast = 0.0f;
    float m_yDiff = 0.0f;
};