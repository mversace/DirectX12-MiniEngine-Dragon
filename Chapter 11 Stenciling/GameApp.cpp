#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"

#include <DirectXColors.h>
#include <fstream>
#include <array>
#include "GameInput.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"

namespace GameCore
{
    extern HWND g_hWnd;
}

void GameApp::Startup(void)
{
    buildRoomGeo();
    buildSkullGeo();
    buildMaterials();
    buildRenderItem();

    // 根签名
    m_RootSignature.Reset(4, 1);
    m_RootSignature.InitStaticSampler(0, Graphics::SamplerAnisoWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);
    m_RootSignature[2].InitAsConstantBuffer(2, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature.Finalize(L"box signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // 创建PSO
    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = Graphics::g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = Graphics::g_SceneDepthBuffer.GetFormat();

    // 默认PSO
    GraphicsPSO defaultPSO;
    defaultPSO.SetRootSignature(m_RootSignature);
    defaultPSO.SetRasterizerState(Graphics::RasterizerDefaultCw);
    defaultPSO.SetBlendState(Graphics::BlendDisable);
    defaultPSO.SetDepthStencilState(Graphics::DepthStateReadWrite);
    defaultPSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    defaultPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    defaultPSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    defaultPSO.SetVertexShader(g_pdefaultVS, sizeof(g_pdefaultVS));
    defaultPSO.SetPixelShader(g_pdefaultPS, sizeof(g_pdefaultPS));
    defaultPSO.Finalize();

    // 默认PSO
    m_mapPSO[E_EPT_DEFAULT] = defaultPSO;
}

void GameApp::Cleanup(void)
{
    m_mapGeometries.clear();
    m_mapMaterial.clear();
    m_mapPSO.clear();
}

void GameApp::Update(float deltaT)
{
    // 在title那里显示渲染帧数
    float fps = Graphics::GetFrameRate(); // fps = frameCnt / 1
    float mspf = Graphics::GetFrameTime();

    std::wstring fpsStr = std::to_wstring(fps);
    std::wstring mspfStr = std::to_wstring(mspf);

    std::wstring windowText = L"CrossGate";
    windowText +=
        L"    fps: " + fpsStr +
        L"   mspf: " + mspfStr;

    SetWindowText(GameCore::g_hWnd, windowText.c_str());

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

    updateSkull(deltaT);
}

void GameApp::updateSkull(float deltaT)
{
    if (GameInput::IsPressed(GameInput::kKey_a))
        mSkullTranslation -= { 1.0f * deltaT, 0.0f, 0.0f };

    if (GameInput::IsPressed(GameInput::kKey_d))
        mSkullTranslation += { 1.0f * deltaT, 0.0f, 0.0f };

    if (GameInput::IsPressed(GameInput::kKey_w))
        mSkullTranslation += { 0.0f, 1.0f * deltaT, 0.0f };

    if (GameInput::IsPressed(GameInput::kKey_s))
        mSkullTranslation -= { 0.0f, 1.0f * deltaT, 0.0f };

    // y坐标不允许低于地板
    float y = (float)mSkullTranslation.GetY();
    if (y < 0.0f)
        mSkullTranslation.SetY(0.0f);

    // 更新最新的skull世界矩阵
    auto rotationMatrix = Math::AffineTransform::MakeYRotation(Math::XM_PIDIV2);
    auto scallMatrix = Math::AffineTransform::MakeScale({ 0.45f, 0.45f, 0.45f });
    auto translateMatrix = Math::AffineTransform::MakeTranslation(mSkullTranslation);
    mSkullRitem->modeToWorld = Math::Transpose(Math::Matrix4(translateMatrix * scallMatrix * rotationMatrix));
}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    gfxContext.ClearColor(Graphics::g_SceneColorBuffer);

    gfxContext.TransitionResource(Graphics::g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(Graphics::g_SceneDepthBuffer);

    gfxContext.SetRenderTarget(Graphics::g_SceneColorBuffer.GetRTV(), Graphics::g_SceneDepthBuffer.GetDSV());

    // 设置渲染流水线
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_DEFAULT]);

    // 设置根签名
    gfxContext.SetRootSignature(m_RootSignature);
    // 设置顶点拓扑结构
    gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置通用的常量缓冲区
    PassConstants psc;
    psc.viewProj = Transpose(m_ViewProjMatrix);
    psc.eyePosW = m_Camera.GetPosition();
    psc.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    psc.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    psc.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    psc.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    psc.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    psc.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    psc.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
    gfxContext.SetDynamicConstantBufferView(1, sizeof(psc), &psc);

    // 开始绘制
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Opaque]);

    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish(true);
}

void GameApp::drawRenderItems(GraphicsContext& gfxContext, std::vector<std::unique_ptr<RenderItem>>& ritems)
{
    for (auto& item : ritems)
    {
        // 设置顶点
        gfxContext.SetVertexBuffer(0, item->geo->vertexView);
        // 设置索引
        gfxContext.SetIndexBuffer(item->geo->indexView);

        // 设置渲染目标的转换矩阵、纹理矩阵、纹理控制矩阵
        ObjectConstants obc;
        obc.World = item->modeToWorld;
        obc.texTransform = item->texTransform;
        obc.matTransform = Transpose(item->matTransform);
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        // 设置渲染目标的纹理视图
        gfxContext.SetDynamicDescriptor(3, 0, item->mat->srv);

        // 设置渲染目标的纹理属性
        MaterialConstants mc;
        mc.DiffuseAlbedo = item->mat->diffuseAlbedo;
        mc.FresnelR0 = item->mat->fresnelR0;
        mc.Roughness = item->mat->roughness;
        gfxContext.SetDynamicConstantBufferView(2, sizeof(mc), &mc);

        gfxContext.DrawIndexed(item->IndexCount, item->StartIndexLocation, item->BaseVertexLocation);
    }
}

void GameApp::buildRoomGeo()
{
    // Create and specify geometry.  For this sample we draw a floor
    // and a wall with a mirror on it.  We put the floor, wall, and
    // mirror geometry in one vertex buffer.
    //
    //   |--------------|
    //   |              |
    //   |----|----|----|
    //   |Wall|Mirr|Wall|
    //   |    | or |    |
    //   /--------------/
    //  /   Floor      /
    // /--------------/

    std::vector<Vertex> vertices =
    {
        // Floor: Observe we tile texture coordinates.
        Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
        Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
        Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

        // Wall: Observe we tile texture coordinates, and that we
        // leave a gap in the middle for the mirror.
        Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
        Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
        Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

        // Mirror
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
    };

    std::vector<std::uint16_t> indices =
    {
        // Floor
        0, 1, 2,
        0, 2, 3,

        // Walls
        4, 5, 6,
        4, 6, 7,

        8, 9, 10,
        8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        // Mirror
        16, 17, 18,
        16, 18, 19
    };

    SubmeshGeometry floorSubmesh;
    floorSubmesh.IndexCount = 6;
    floorSubmesh.StartIndexLocation = 0;
    floorSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry wallSubmesh;
    wallSubmesh.IndexCount = 18;
    wallSubmesh.StartIndexLocation = 6;
    wallSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry mirrorSubmesh;
    mirrorSubmesh.IndexCount = 6;
    mirrorSubmesh.StartIndexLocation = 24;
    mirrorSubmesh.BaseVertexLocation = 0;

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "roomGeo";

    geo->createVertex(L"roomGeo vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"roomGeo index", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    geo->geoMap["floor"] = floorSubmesh;
    geo->geoMap["wall"] = wallSubmesh;
    geo->geoMap["mirror"] = mirrorSubmesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildSkullGeo()
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

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        // Model does not have texture coordinates, so just zero them out.
        vertices[i].TexC = { 0.0f, 0.0f };
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

    geo->createVertex(L"skull vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"skull index", (UINT)indices.size(), sizeof(std::int32_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["skull"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildMaterials()
{
    TextureManager::Initialize(L"Textures/");

    auto bricks = std::make_unique<Material>();
    bricks->name = "bricks";
    bricks->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    bricks->fresnelR0 = { 0.05f, 0.05f, 0.05f };
    bricks->roughness = 0.25f;
    bricks->srv = TextureManager::LoadFromFile(L"bricks3", true)->GetSRV();

    auto checkertile = std::make_unique<Material>();
    checkertile->name = "checkertile";
    checkertile->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    checkertile->fresnelR0 = { 0.07f, 0.07f, 0.07f };
    checkertile->roughness = 0.3f;
    checkertile->srv = TextureManager::LoadFromFile(L"checkboard", true)->GetSRV();

    auto icemirror = std::make_unique<Material>();
    icemirror->name = "icemirror";
    icemirror->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.3f };
    icemirror->fresnelR0 = { 0.1f, 0.1f, 0.1f };
    icemirror->roughness = 0.5f;
    icemirror->srv = TextureManager::LoadFromFile(L"ice", true)->GetSRV();

    auto skullMat = std::make_unique<Material>();
    skullMat->name = "skullMat";
    skullMat->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    skullMat->fresnelR0 = { 0.05f, 0.05f, 0.05f };
    skullMat->roughness = 0.3f;
    skullMat->srv = TextureManager::LoadFromFile(L"white1x1", true)->GetSRV();

    m_mapMaterial["bricks"] = std::move(bricks);
    m_mapMaterial["checkertile"] = std::move(checkertile);
    m_mapMaterial["icemirror"] = std::move(icemirror);
    m_mapMaterial["skullMat"] = std::move(skullMat);
}

void GameApp::buildRenderItem()
{
    // 地板
    auto floorRItem = std::make_unique<RenderItem>();
    floorRItem->IndexCount = m_mapGeometries["roomGeo"]->geoMap["floor"].IndexCount;
    floorRItem->StartIndexLocation = m_mapGeometries["roomGeo"]->geoMap["floor"].StartIndexLocation;
    floorRItem->BaseVertexLocation = m_mapGeometries["roomGeo"]->geoMap["floor"].BaseVertexLocation;
    floorRItem->geo = m_mapGeometries["roomGeo"].get();
    floorRItem->mat = m_mapMaterial["checkertile"].get();
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(std::move(floorRItem));

    // 墙壁
    auto wallRItem = std::make_unique<RenderItem>();
    wallRItem->IndexCount = m_mapGeometries["roomGeo"]->geoMap["wall"].IndexCount;
    wallRItem->StartIndexLocation = m_mapGeometries["roomGeo"]->geoMap["wall"].StartIndexLocation;
    wallRItem->BaseVertexLocation = m_mapGeometries["roomGeo"]->geoMap["wall"].BaseVertexLocation;
    wallRItem->geo = m_mapGeometries["roomGeo"].get();
    wallRItem->mat = m_mapMaterial["bricks"].get();
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(std::move(wallRItem));

    // 镜子
    auto mirrorRItem = std::make_unique<RenderItem>();
    mirrorRItem->IndexCount = m_mapGeometries["roomGeo"]->geoMap["mirror"].IndexCount;
    mirrorRItem->StartIndexLocation = m_mapGeometries["roomGeo"]->geoMap["mirror"].StartIndexLocation;
    mirrorRItem->BaseVertexLocation = m_mapGeometries["roomGeo"]->geoMap["mirror"].BaseVertexLocation;
    mirrorRItem->geo = m_mapGeometries["roomGeo"].get();
    mirrorRItem->mat = m_mapMaterial["icemirror"].get();
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(std::move(mirrorRItem));

    // skull
    auto skullRItem = std::make_unique<RenderItem>();
    skullRItem->IndexCount = m_mapGeometries["skullGeo"]->geoMap["skull"].IndexCount;
    skullRItem->StartIndexLocation = m_mapGeometries["skullGeo"]->geoMap["skull"].StartIndexLocation;
    skullRItem->BaseVertexLocation = m_mapGeometries["skullGeo"]->geoMap["skull"].BaseVertexLocation;
    skullRItem->geo = m_mapGeometries["skullGeo"].get();
    skullRItem->mat = m_mapMaterial["skullMat"].get();
    mSkullRitem = skullRItem.get();
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(std::move(skullRItem));
}