#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "GameInput.h"

#include "GeometryGenerator.h"
#include "CompiledShaders/dynamicIndexDefaultPS.h"
#include "CompiledShaders/dynamicIndexDefaultVS.h"

void GameApp::Startup(void)
{
    buildPSO();
    buildGeo();
    buildMaterials();
    buildRenderItem();

    m_Camera.SetEyeAtUp({ 0.0f, 2.0f, -15.0f }, { 0.0f, 2.0f, 0.0f }, Math::Vector3(Math::kYUnitVector));
    m_Camera.SetZRange(1.0f, 10000.0f);
    m_CameraController.reset(new GameCore::CameraController(m_Camera, Math::Vector3(Math::kYUnitVector)));
}

void GameApp::Cleanup(void)
{
    m_mapPSO.clear();

    m_mapGeometries.clear();
    m_mapMaterial.clear();
    m_vecAll.clear();

    for (auto& v : m_vecRenderItems)
        v.clear();
}

void GameApp::Update(float deltaT)
{
    //cameraUpdate();
    m_CameraController->Update(deltaT);

    m_ViewProjMatrix = m_Camera.GetViewProjMatrix();

    // 视口
    m_MainViewport.Width = (float)Graphics::g_SceneColorBuffer.GetWidth();
    m_MainViewport.Height = (float)Graphics::g_SceneColorBuffer.GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    // 裁剪矩形
    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)Graphics::g_SceneColorBuffer.GetWidth();
    m_MainScissor.bottom = (LONG)Graphics::g_SceneColorBuffer.GetHeight();
}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    gfxContext.ClearColor(Graphics::g_SceneColorBuffer);

    gfxContext.TransitionResource(Graphics::g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepthAndStencil(Graphics::g_SceneDepthBuffer);

    gfxContext.SetRenderTarget(Graphics::g_SceneColorBuffer.GetRTV(), Graphics::g_SceneDepthBuffer.GetDSV());

    gfxContext.SetRootSignature(m_RootSignature);

    
    
    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& ritems)
{
    for (auto& item : ritems)
    {
        // 设置顶点
        gfxContext.SetVertexBuffer(0, item->geo->vertexView);

        // 设置索引
        gfxContext.SetIndexBuffer(item->geo->indexView);

        // 设置顶点拓扑结构
        gfxContext.SetPrimitiveTopology(item->PrimitiveType);

        // 设置渲染目标的转换矩阵、纹理矩阵、纹理控制矩阵
        ObjectConstants obc;
        obc.World = item->modeToWorld;
        obc.texTransform = item->texTransform;
        obc.matTransform = item->matTransform;
        obc.MaterialIndex = item->ObjCBIndex;
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        gfxContext.DrawIndexed(item->IndexCount, item->StartIndexLocation, item->BaseVertexLocation);
    }
}

void GameApp::buildPSO()
{
    
}

void GameApp::buildGeo()
{
    
}

inline void GameApp::makeMaterials(const std::string& name, const Math::Vector4& diffuseAlbedo, const Math::Vector3& fresnelR0,
    const float roughness, const std::string& materialName, int idx)
{
    std::wstring strFile(materialName.begin(), materialName.end());
    auto item = std::make_unique<Material>(); 
    item->name = name;
    item->diffuseAlbedo = diffuseAlbedo;
    item->fresnelR0 = fresnelR0;
    item->roughness = roughness;
    item->DiffuseMapIndex = idx;
    m_mapMaterial[name] = std::move(item);
}

void GameApp::buildMaterials()
{
    
}

void GameApp::buildRenderItem()
{
    
}

void GameApp::cameraUpdate()
{
    // 鼠标左键旋转
    if (GameInput::IsPressed(GameInput::kMouse0)) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = GameInput::GetAnalogInput(GameInput::kAnalogMouseX) - m_xLast;
        float dy = GameInput::GetAnalogInput(GameInput::kAnalogMouseY) - m_yLast;

        if (GameInput::IsPressed(GameInput::kMouse0))
        {
            // Update angles based on input to orbit camera around box.
            m_xRotate += (dx - m_xDiff);
            m_yRotate += (dy - m_yDiff);
            m_yRotate = (std::max)(-0.0f + 0.1f, m_yRotate);
            m_yRotate = (std::min)(XM_PIDIV2 - 0.1f, m_yRotate);
        }

        m_xDiff = dx;
        m_yDiff = dy;

        m_xLast += GameInput::GetAnalogInput(GameInput::kAnalogMouseX);
        m_yLast += GameInput::GetAnalogInput(GameInput::kAnalogMouseY);
    }
    else
    {
        m_xDiff = 0.0f;
        m_yDiff = 0.0f;
        m_xLast = 0.0f;
        m_yLast = 0.0f;
    }

    // 滚轮消息，放大缩小
    if (float fl = GameInput::GetAnalogInput(GameInput::kAnalogMouseScroll))
    {
        if (fl > 0)
            m_radius -= 5;
        else
            m_radius += 5;
    }

    // 调整摄像机位置
    // 以(0, 0, -m_radius) 为初始位置
    float x = m_radius * cosf(m_yRotate) * sinf(m_xRotate);
    float y = m_radius * sinf(m_yRotate);
    float z = -m_radius * cosf(m_yRotate) * cosf(m_xRotate);

    m_Camera.SetEyeAtUp({ x, y, z }, Math::Vector3(Math::kZero), Math::Vector3(Math::kYUnitVector));
    m_Camera.Update();
}