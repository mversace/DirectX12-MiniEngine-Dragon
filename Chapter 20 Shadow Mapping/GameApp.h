#pragma once

#include <unordered_map>
#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"
#include "d3dUtil.h"
#include "CameraController.h"

class RootSignature;
class GraphicsPSO;
class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;
    virtual void RenderUI(class GraphicsContext& gfxContext) override;

private:
    void cameraUpdate();   // camera更新

private:
    void buildPSO();
    void buildGeo();
    void buildMaterials();
    void buildRenderItem();
    void drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& ritems);
    
    void buildCubeCamera(float x, float y, float z);
    void DrawSceneToCubeMap(GraphicsContext& gfxContext);

private:
    void buildShapeGeo();
    void buildSkullGeo();

private:
    // 几何结构map
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_mapGeometries;

    // 渲染队列
    enum class RenderLayer : int
    {
        Opaque = 0,
        OpaqueDynamicReflectors,
        Sky,
        Count
    };
    std::vector<RenderItem*> m_vecRenderItems[(int)RenderLayer::Count];
    std::vector<std::unique_ptr<RenderItem>> m_vecAll;

    enum eMaterialType
    {
        bricks = 0,
        tile,
        mirror,
        skull,
        sky
    };
    StructuredBuffer m_mats;    // t1 存储所有的纹理属性
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_srvs;  // 存储所有的纹理资源

private:
    // 根签名
    RootSignature m_RootSignature;

    // 渲染流水线
    enum ePSOType
    {
        E_EPT_DEFAULT = 1,
        E_EPT_SKY,
    };
    std::unordered_map<int, GraphicsPSO> m_mapPSO;

    RenderItem* m_SkullRItem = nullptr;

    // 天空盒摄像机
    Math::Camera m_CameraCube[6];

    // 摄像机
    // 以(0, 0, -m_radius) 为初始位置
    Math::Camera m_Camera;
    Math::Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    // 摄像机控制器
    std::unique_ptr<GameCore::CameraController> m_CameraController;

    // 半径
    float m_radius = 10.0f;

    // x方向弧度，摄像机的x坐标增加，则m_xRotate增加
    float m_xRotate = -Math::XM_PIDIV4 / 2.0f;
    float m_xLast = 0.0f;
    float m_xDiff = 0.0f;

    // y方向弧度，摄像机y坐标增加，则m_yRotate增加
    // m_yRotate范围 [-XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f]
    float m_yRotate = Math::XM_PIDIV4 / 2.0f;
    float m_yLast = 0.0f;
    float m_yDiff = 0.0f;
};