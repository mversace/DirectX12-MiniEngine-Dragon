#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"
#include "Waves.h"
#include "d3dUtil.h"
#include <unordered_map>

class RootSignature;
class StructuredBuffer;
class ByteAddressBuffer;
class GraphicsPSO;
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
        Matrix4 texTransform;
        Matrix4 matTransform;
        int indexCount;
        int startIndex;
        int baseVertex;
        XMFLOAT4 diffuseAlbedo;
        XMFLOAT3 fresnelR0;
        float roughness;
        D3D12_CPU_DESCRIPTOR_HANDLE srv;
    };
    void buildShapesData();
    void renderShapes(GraphicsContext& gfxContext);

    void buildLandAndWaves();
    float GetHillsHeight(float x, float z) const;
    XMFLOAT3 GetHillsNormal(float x, float z) const;
    void renderLandAndWaves(GraphicsContext& gfxContext);
    void UpdateWaves(float deltaT);
    void AnimateMaterials(float deltaT);
    void setShaderParam(GraphicsContext& gfxContext, renderItem& item);

private:
    // 顶点结构体
    struct Vertex
    {
        XMFLOAT3 Pos;
        XMFLOAT3 Normal;
        XMFLOAT2 TexC;
    };

    RootSignature m_RootSignature;

    enum ePSOType
    {
        E_EPT_DEFAULT = 1,
        E_EPT_WIREFRAME = 2
    };
    std::unordered_map<int, GraphicsPSO> m_mapPSO;

    bool m_bRenderShapes = true;
    bool m_bRenderFill = true;

    // shapes
    StructuredBuffer m_VertexBuffer;
    ByteAddressBuffer m_IndexBuffer;
    std::vector<renderItem> m_vecShapes;
    // skull
    StructuredBuffer m_VertexBufferSkull;
    ByteAddressBuffer m_IndexBufferSkull;

    // land and waves
    StructuredBuffer m_VertexBufferLand;
    ByteAddressBuffer m_IndexBufferLand;
    renderItem m_renderItemLand;

    Waves m_waves{ 128, 128, 1.0f, 0.03f, 4.0f, 0.2f };
    ByteAddressBuffer m_IndexBufferWaves;
    std::vector<Vertex> m_verticesWaves;
    renderItem m_renderItemWaves;
    // box
    StructuredBuffer m_VertexBufferBox;
    ByteAddressBuffer m_IndexBufferBox;
    renderItem m_renderItemBox;
    float mSunTheta = 0.25f * XM_PI;
    float mSunPhi = XM_PIDIV4;
    
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