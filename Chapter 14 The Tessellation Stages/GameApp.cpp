#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "GameInput.h"

#include "CompiledShaders/tessVS.h"
#include "CompiledShaders/tessHS.h"
#include "CompiledShaders/tessDS.h"
#include "CompiledShaders/tessPS.h"

void GameApp::Startup(void)
{
    buildQuadPatchGeo();
    buildRenderItem();

    // 创建根签名
    m_RootSignature.Reset(2, 0);
    m_RootSignature[0].InitAsConstantBuffer(0);
    m_RootSignature[1].InitAsConstantBuffer(1);
    m_RootSignature.Finalize(L"tess RS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // 创建PSO
    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = Graphics::g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = Graphics::g_SceneDepthBuffer.GetFormat();

    // 画线
    auto raster = Graphics::RasterizerDefaultCw;
    raster.FillMode = D3D12_FILL_MODE_WIREFRAME;

    // 默认PSO
    GraphicsPSO defaultPSO;
    defaultPSO.SetRootSignature(m_RootSignature);
    defaultPSO.SetRasterizerState(raster);
    defaultPSO.SetBlendState(Graphics::BlendDisable);
    defaultPSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
    defaultPSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    defaultPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
    defaultPSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    defaultPSO.SetVertexShader(g_ptessVS, sizeof(g_ptessVS));
    defaultPSO.SetHullShader(g_ptessHS, sizeof(g_ptessHS));
    defaultPSO.SetDomainShader(g_ptessDS, sizeof(g_ptessDS));
    defaultPSO.SetPixelShader(g_ptessPS, sizeof(g_ptessPS));
    defaultPSO.Finalize();

    // 默认PSO
    m_mapPSO[E_EPT_DEFAULT] = defaultPSO;
}

void GameApp::Cleanup(void)
{
    m_mapPSO.clear();

    m_mapGeometries.clear();
    m_vecAll.clear();
}

void GameApp::Update(float deltaT)
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
    float x = m_radius* cosf(m_yRotate)* sinf(m_xRotate);
    float y = m_radius* sinf(m_yRotate);
    float z = -m_radius* cosf(m_yRotate)* cosf(m_xRotate);

    m_Camera.SetEyeAtUp({ x, y, z }, Math::Vector3(Math::kZero), Math::Vector3(Math::kYUnitVector));
    m_Camera.Update();

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

    // 设置通用的常量缓冲区
    PassConstants psc;
    psc.viewProj = Transpose(m_ViewProjMatrix);
    psc.eyePosW = m_Camera.GetPosition();
    gfxContext.SetDynamicConstantBufferView(1, sizeof(psc), &psc);

    gfxContext.SetPipelineState(m_mapPSO[E_EPT_DEFAULT]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Opaque]);
    
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
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        gfxContext.DrawIndexed(item->IndexCount, item->StartIndexLocation, item->BaseVertexLocation);
    }
}

void GameApp::buildQuadPatchGeo()
{
    std::vector<XMFLOAT3> vertices =
    {
        XMFLOAT3(-10.0f, 0.0f, +10.0f),
        XMFLOAT3(+10.0f, 0.0f, +10.0f),
        XMFLOAT3(-10.0f, 0.0f, -10.0f),
        XMFLOAT3(+10.0f, 0.0f, -10.0f)
    };

    std::vector<std::int16_t> indices = { 0, 1, 2, 3 };

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "quadpatchGeo";

    geo->createVertex(L"quadpatchGeo vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"quadpatchGeo index", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["quadpatch"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildRenderItem()
{
    auto quadPatchRitem = std::make_unique<RenderItem>();
    quadPatchRitem->geo = m_mapGeometries["quadpatchGeo"].get();
    quadPatchRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
    quadPatchRitem->IndexCount = quadPatchRitem->geo->geoMap["quadpatch"].IndexCount;
    quadPatchRitem->StartIndexLocation = quadPatchRitem->geo->geoMap["quadpatch"].StartIndexLocation;
    quadPatchRitem->BaseVertexLocation = quadPatchRitem->geo->geoMap["quadpatch"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(quadPatchRitem.get());

    m_vecAll.push_back(std::move(quadPatchRitem));
}