#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "GameInput.h"

#include <fstream>
#include <sstream>
#include "GeometryGenerator.h"
#include "CompiledShaders/dynamicIndexDefaultPS.h"
#include "CompiledShaders/dynamicIndexDefaultVS.h"

void GameApp::Startup(void)
{
    buildPSO();
    buildGeo();
    buildMaterials();
    buildRenderItem();

    m_Camera.SetEyeAtUp({ 0.0f, 0.0f, -15.0f }, { 0.0f, 0.0f, 0.0f }, Math::Vector3(Math::kYUnitVector));
    m_CameraController.reset(new GameCore::CameraController(m_Camera, Math::Vector3(Math::kYUnitVector)));
}

void GameApp::Cleanup(void)
{
    m_mapPSO.clear();

    m_mapGeometries.clear();
    m_vecAll.clear();

    for (auto& v : m_vecRenderItems)
        v.clear();

    m_mats.Destroy();
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

    updateInstanceData();
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
    psc.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    psc.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    psc.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    psc.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    psc.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    psc.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    psc.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
    gfxContext.SetDynamicConstantBufferView(0, sizeof(psc), &psc);

    // 设置全部的纹理参数
    gfxContext.SetBufferSRV(3, m_mats);

    // 设置全部的纹理资源
    gfxContext.SetDynamicDescriptors(4, 0, 7, &m_srvs[0]);

    gfxContext.SetPipelineState(m_mapPSO[E_EPT_DEFAULT]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Opaque]);
    
    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::RenderUI(class GraphicsContext& gfxContext)
{
    TextContext Text(gfxContext);
    Text.Begin();

    Text.ResetCursor(Graphics::g_DisplayWidth / 2.0f, 5.0f);
    Text.SetColor(Color(0.0f, 1.0f, 0.0f));

    for (auto& e : m_vecAll)
    {
        std::stringstream ss;
        ss << e->name << " : " << e->visibileCount << " / " << e->allCount << "\n";

        Text.DrawString(ss.str());
    }
    
    Text.SetTextSize(20.0f);

    Text.End();
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

        // 设置该绘制目标需要的纹理数据
        gfxContext.SetBufferSRV(2, item->matrixs);

        // 设置需要绘制的目标索引
        gfxContext.SetDynamicConstantBufferView(1, item->vDrawObjs.size() * sizeof(item->vDrawObjs[0]), item->vDrawObjs.data());
        
        gfxContext.DrawIndexedInstanced(item->IndexCount, item->visibileCount, item->StartIndexLocation, item->BaseVertexLocation, 0);
    }
}

void GameApp::buildPSO()
{
    // 创建根签名
    m_RootSignature.Reset(5, 1);
    m_RootSignature.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc);
    m_RootSignature[0].InitAsConstantBuffer(0);     // 通用的常量缓冲区
    m_RootSignature[1].InitAsConstantBuffer(1);     // 可渲染目标数组
    m_RootSignature[2].InitAsBufferSRV(0);          // 渲染目标的矩阵数据
    m_RootSignature[3].InitAsBufferSRV(1);          // 渲染目标的纹理属性
    m_RootSignature[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 7);    // 渲染目标的纹理
    m_RootSignature.Finalize(L"16 RS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // 创建PSO
    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    DXGI_FORMAT ColorFormat = Graphics::g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = Graphics::g_SceneDepthBuffer.GetFormat();

    GraphicsPSO defaultPSO;
    defaultPSO.SetRootSignature(m_RootSignature);
    defaultPSO.SetRasterizerState(Graphics::RasterizerDefaultCw);
    defaultPSO.SetBlendState(Graphics::BlendDisable);
    defaultPSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
    defaultPSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    defaultPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    defaultPSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    defaultPSO.SetVertexShader(g_pdynamicIndexDefaultVS, sizeof(g_pdynamicIndexDefaultVS));
    defaultPSO.SetPixelShader(g_pdynamicIndexDefaultPS, sizeof(g_pdynamicIndexDefaultPS));
    defaultPSO.Finalize();

    // 默认PSO
    m_mapPSO[E_EPT_DEFAULT] = defaultPSO;
}

void GameApp::buildGeo()
{
    std::ifstream fin("Models/skull.txt");

    if (!fin)
    {
        MessageBox(0, L"Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    Math::Vector3 vMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    Math::Vector3 vMax = { FLT_MIN, FLT_MIN, FLT_MIN };

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

        // 计算纹理坐标
        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        float theta = atan2f(spherePos.z, spherePos.x);

        // Put in [0, 2pi].
        if (theta < 0.0f)
            theta += XM_2PI;

        float phi = acosf(spherePos.y);

        float u = theta / (2.0f * XM_PI);
        float v = phi / XM_PI;

        vertices[i].TexC = { u, v };

        vMin = Math::Min(vMin, Math::Vector3(P));
        vMax = Math::Max(vMax, Math::Vector3(P));
    }

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "skullGeo";

    geo->createVertex(L"skullGeo vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"skullGeo index", (UINT)indices.size(), sizeof(std::int32_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.vMin = vMin;
    submesh.vMax = vMax;

    geo->geoMap["skull"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildMaterials()
{
    std::vector<MaterialConstants> v = {
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.02f, 0.02f, 0.02f }, 0.1f, 0 },   // bricks
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.3f, 1 },   // stone
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.02f, 0.02f, 0.02f }, 0.3f, 2 },   // tile
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.2f, 3 },   // checkboard
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.10f, 0.10f, 0.10f }, 0.0f, 4 },   // ice
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.2f, 5 },   // grass
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.5f, 6 }   // skull
    };

    // 存入所有纹理属性
    m_mats.Create(L"skull mats", (UINT)v.size(), sizeof(MaterialConstants), v.data());

    m_srvs.resize(7);
    TextureManager::Initialize(L"Textures/");
    m_srvs[0] = TextureManager::LoadFromFile(L"bricks", true)->GetSRV();
    m_srvs[1] = TextureManager::LoadFromFile(L"stone", true)->GetSRV();
    m_srvs[2] = TextureManager::LoadFromFile(L"tile", true)->GetSRV();
    m_srvs[3] = TextureManager::LoadFromFile(L"WoodCrate01", true)->GetSRV();
    m_srvs[4] = TextureManager::LoadFromFile(L"ice", true)->GetSRV();
    m_srvs[5] = TextureManager::LoadFromFile(L"grass", true)->GetSRV();
    m_srvs[6] = TextureManager::LoadFromFile(L"white1x1", true)->GetSRV();
}

void GameApp::buildRenderItem()
{
    // 绘制125个
    int n = 5;
    int nInstanceCount = n * n * n;

    int maxCount = Math::AlignUp(nInstanceCount * 4, 16) / 4;

    auto skullRitem = std::make_unique<RenderItem>();
    skullRitem->name = "skull";
    skullRitem->visibileCount = nInstanceCount;
    skullRitem->allCount = nInstanceCount;
    skullRitem->vDrawObjs.resize(maxCount);
    skullRitem->geo = m_mapGeometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->IndexCount = skullRitem->geo->geoMap["skull"].IndexCount;
    skullRitem->StartIndexLocation = skullRitem->geo->geoMap["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->geo->geoMap["skull"].BaseVertexLocation;
    skullRitem->vMin = skullRitem->geo->geoMap["skull"].vMin;
    skullRitem->vMax = skullRitem->geo->geoMap["skull"].vMax;

    // 为这个绘制目标添加nInstanceCount个矩阵数据，以分布在不同的世界位置
    skullRitem->vObjsData.resize(nInstanceCount);
    float width = 200.0f;
    float height = 200.0f;
    float depth = 200.0f;

    float x = -0.5f * width;
    float y = -0.5f * height;
    float z = -0.5f * depth;
    float dx = width / (n - 1);
    float dy = height / (n - 1);
    float dz = depth / (n - 1);
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                int index = k * n * n + i * n + j;
                // Position instanced along a 3D grid.
                skullRitem->vObjsData[index].World = Math::Transpose(Math::Matrix4(
                    { 1.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f },
                    { x + j * dx, y + i * dy, z + k * dz }
                ));
                skullRitem->vObjsData[index].texTransform = Math::Transpose(Math::Matrix4::MakeScale({2.0f, 2.0f, 1.0f}));
                skullRitem->vObjsData[index].MaterialIndex = index % m_mats.GetElementCount();
            }
        }
    }

    skullRitem->matrixs.Create(L"skull matrixs", nInstanceCount, sizeof(ObjectConstants), skullRitem->vObjsData.data());
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(skullRitem.get());
    m_vecAll.push_back(std::move(skullRitem));
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

void GameApp::updateInstanceData()
{
    for (auto& e : m_vecAll)
    {
        e->visibileCount = 0;
        for (int i = 0; i < (int)e->vObjsData.size(); ++i)
        {
            if (g_openFrustumCull)
            {
                auto& item = e->vObjsData[i];
                auto vMin = Math::Vector3(Math::Transpose(item.World) * e->vMin);
                auto vMax = Math::Vector3(Math::Transpose(item.World) * e->vMax);
                if (m_Camera.GetWorldSpaceFrustum().IntersectBoundingBox(vMin, vMax))
                {
                    e->vDrawObjs[e->visibileCount++].x = i;
                }
            }
            else
            {
                e->vDrawObjs[e->visibileCount++].x = i;
            }
        }
    }
}