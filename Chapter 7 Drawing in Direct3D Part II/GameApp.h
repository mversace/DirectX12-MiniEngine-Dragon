#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"
#include "Waves.h"

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
    struct renderItem
    {
        Matrix4 modelToWorld;
        int indexCount;
        int startIndex;
        int baseVertex;
    };
    void buildShapesData();
    void renderShapes(GraphicsContext& gfxContext);

    void buildLandAndWaves();
    float GetHillsHeight(float x, float z) const;
    void renderLandAndWaves(GraphicsContext& gfxContext);
    void UpdateWaves(float deltaT);

private:
    // 顶点结构体
    struct Vertex
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Color;
    };

    RootSignature m_RootSignature;
    GraphicsPSO m_PSO;
    GraphicsPSO m_PSOEx;

    bool m_bRenderShapes = true;
    bool m_bRenderFill = false;

    // shapes
    StructuredBuffer m_VertexBuffer;
    ByteAddressBuffer m_IndexBuffer;
    std::vector<renderItem> m_vecShapes;
    // land and waves
    StructuredBuffer m_VertexBufferLand;
    ByteAddressBuffer m_IndexBufferLand;
    Waves m_waves{ 128, 128, 1.0f, 0.03f, 4.0f, 0.2f };
    ByteAddressBuffer m_IndexBufferWaves;
    std::vector<Vertex> m_verticesWaves;
    
    // 摄像机
    Camera m_Camera;
    Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    // 半径
    float m_radius = 22.0f;
    // x方向弧度
    float m_xRotate = -Math::XM_PIDIV2;
    float m_xLast = 0.0f;
    float m_xDiff = 0.0f;
    // y方向弧度
    float m_yRotate = Math::XM_PIDIV4;
    float m_yLast = 0.0f;
    float m_yDiff = 0.0f;
};