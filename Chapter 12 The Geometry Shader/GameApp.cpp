#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "GeometryGenerator.h"

#include <DirectXColors.h>
#include <fstream>
#include <array>
#include "GameInput.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"
#include "CompiledShaders/billboardVS.h"
#include "CompiledShaders/billboardGS.h"
#include "CompiledShaders/billboardPS.h"

namespace GameCore
{
    extern HWND g_hWnd;
}

static float RandF()
{
    return (float)(rand()) / (float)RAND_MAX;
}

// Returns random float in [a, b).
static float RandF(float a, float b)
{
    return a + RandF() * (b - a);
}

static int Rand(int a, int b)
{
    return a + rand() % ((b - a) + 1);
}

void GameApp::Startup(void)
{
    buildLandGeo();
    buildBoxGeo();
    buildWavesGeo();
    buildTreeGeo();
    buildMaterials();
    buildRenderItem();

    Graphics::g_SceneColorBuffer.SetClearColor({ 0.7f, 0.7f, 0.7f });

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

    // alpha test PSO
    // 对于本例箱子一些面是透明的，要求显示到背面的东西
    GraphicsPSO alphaTestPSO = defaultPSO;
    auto raster = Graphics::RasterizerDefaultCw;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    alphaTestPSO.SetRasterizerState(raster);
    alphaTestPSO.Finalize();
    m_mapPSO[E_EPT_ALPHATEST] = alphaTestPSO;

    // 透明PSO 绘制半透明的镜子
    GraphicsPSO transparencyPSO = defaultPSO;
    auto blend = Graphics::BlendTraditional;
    // 目标的alpha用0，这样透过去的目标就不再变淡了
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO; 
    transparencyPSO.SetBlendState(blend);
    transparencyPSO.Finalize();
    m_mapPSO[E_EPT_TRANSPARENT] = transparencyPSO;

    // 公告板PSO
    GraphicsPSO billboardPSO = defaultPSO;
    billboardPSO.SetRasterizerState(Graphics::RasterizerDefaultCwMsaa);
    blend = Graphics::BlendDisable;
    blend.AlphaToCoverageEnable = true;
    billboardPSO.SetBlendState(blend);
    D3D12_INPUT_ELEMENT_DESC mInputLayoutBB[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    billboardPSO.SetInputLayout(_countof(mInputLayoutBB), mInputLayoutBB);
    billboardPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
    billboardPSO.SetVertexShader(g_pbillboardVS, sizeof(g_pbillboardVS));
    billboardPSO.SetGeometryShader(g_pbillboardGS, sizeof(g_pbillboardGS));
    billboardPSO.SetPixelShader(g_pbillboardPS, sizeof(g_pbillboardPS));
    billboardPSO.Finalize();
    m_mapPSO[E_EPT_BILLBOARD] = billboardPSO;
}

void GameApp::Cleanup(void)
{
    m_mapGeometries.clear();
    m_mapMaterial.clear();
    m_mapPSO.clear();

    m_vecAll.clear();
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

    UpdateWaves(deltaT);
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

    // 设置根签名
    gfxContext.SetRootSignature(m_RootSignature);
   
    // 设置显示物体的常量缓冲区
    setLightContantsBuff(gfxContext);

    // 开始绘制
    // 绘制陆地
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_DEFAULT]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Opaque]);

    // 绘制箱子
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_ALPHATEST]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::AlphaTest]);

    // 绘制公告板的树
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_BILLBOARD]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::AlphaTestedTreeSprites]);

    // 绘制水体
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_TRANSPARENT]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Transparent]);

    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& ritems)
{
    for (auto& item : ritems)
    {
        // 设置顶点
        if (item->geo->bDynamicVertex)
        {
            gfxContext.SetDynamicVB(0, item->geo->vecVertex.size(), sizeof(Vertex), item->geo->vecVertex.data());
        }
        else
        {
            gfxContext.SetVertexBuffer(0, item->geo->vertexView);
        }
        
        // 设置索引
        gfxContext.SetIndexBuffer(item->geo->indexView);

        // 设置顶点拓扑结构
        gfxContext.SetPrimitiveTopology(item->PrimitiveType);

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

void GameApp::setLightContantsBuff(GraphicsContext& gfxContext)
{
    // 设置通用的常量缓冲区
    PassConstants psc;
    psc.viewProj = Transpose(m_ViewProjMatrix);
    psc.eyePosW = m_Camera.GetPosition();
    psc.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    psc.Lights[0].Direction = { 0.0f, -1.0f, -1.0f };
    psc.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };
    gfxContext.SetDynamicConstantBufferView(1, sizeof(psc), &psc);
}

void GameApp::buildLandGeo()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
        vertices[i].TexC = grid.Vertices[i].TexC;
    }

    std::vector<std::uint16_t> indices = grid.GetIndices16();

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "landGeo";

    geo->createVertex(L"landGeo vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"landGeo index", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["land"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildBoxGeo()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);
    auto totalVertexCount = box.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "boxGeo";

    geo->createVertex(L"boxGeo vertex", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"boxGeo index", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["box"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildWavesGeo()
{
    // waves
    std::vector<std::uint16_t> waveIndices(3 * m_waves.TriangleCount()); // 3 indices per face

    // Iterate over each quad.
    int m = m_waves.RowCount();
    int n = m_waves.ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            waveIndices[k] = i * n + j;
            waveIndices[k + 1] = i * n + j + 1;
            waveIndices[k + 2] = (i + 1) * n + j;

            waveIndices[k + 3] = (i + 1) * n + j;
            waveIndices[k + 4] = i * n + j + 1;
            waveIndices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "wavesGeo";
    geo->bDynamicVertex = true;

    geo->vecVertex.resize(m_waves.VertexCount());
    geo->createIndex(L"wavesGeo index", (UINT)waveIndices.size(), sizeof(std::uint16_t), waveIndices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)waveIndices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["wave"] = submesh;

    m_pWavesVec = &geo->vecVertex;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildTreeGeo()
{
    struct TreeSpriteVertex
    {
        XMFLOAT3 Pos;
        XMFLOAT2 Size;
    };

    static const int treeCount = 16;
    std::vector<TreeSpriteVertex> vertices(treeCount);
    for (UINT i = 0; i < treeCount; ++i)
    {
        float x = RandF(-45.0f, 45.0f);
        float z = RandF(-45.0f, 45.0f);
        float y = GetHillsHeight(x, z);

        // Move tree slightly above land height.
        y += 8.0f;

        vertices[i].Pos = XMFLOAT3(x, y, z);
        vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
    }

    std::vector<std::uint16_t> indices(treeCount);
    for (int i = 0; i < treeCount; ++i)
        indices[i] = i;

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "treeSpritesGeo";

    geo->createVertex(L"treeGeo vertex", (UINT)vertices.size(), sizeof(TreeSpriteVertex), vertices.data());
    geo->createIndex(L"treeGeo index", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["points"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildMaterials()
{
    TextureManager::Initialize(L"Textures/");

    auto grass = std::make_unique<Material>();
    grass->name = "grass";
    grass->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    grass->fresnelR0 = { 0.01f, 0.01f, 0.01f };
    grass->roughness = 0.1255f;
    grass->srv = TextureManager::LoadFromFile(L"grass", true)->GetSRV();

    auto water = std::make_unique<Material>();
    water->name = "water";
    water->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.5f };
    water->fresnelR0 = { 0.12f, 0.1f, 0.1f };
    water->roughness = 0.0f;
    water->srv = TextureManager::LoadFromFile(L"water1", true)->GetSRV();

    auto wireFence = std::make_unique<Material>();
    wireFence->name = "wireFence";
    wireFence->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 0.3f };
    wireFence->fresnelR0 = { 0.1f, 0.1f, 0.1f };
    wireFence->roughness = 0.25f;
    wireFence->srv = TextureManager::LoadFromFile(L"WireFence", true)->GetSRV();

    auto treeSprites = std::make_unique<Material>();
    treeSprites->name = "treeSprites";
    treeSprites->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    treeSprites->fresnelR0 = { 0.01f, 0.01f, 0.01f };
    treeSprites->roughness = 0.125f;
    treeSprites->srv = TextureManager::LoadFromFile(L"treeArray2", true)->GetSRV();

    m_mapMaterial[grass->name] = std::move(grass);
    m_mapMaterial[water->name] = std::move(water);
    m_mapMaterial[wireFence->name] = std::move(wireFence);
    m_mapMaterial[treeSprites->name] = std::move(treeSprites);
}

void GameApp::buildRenderItem()
{
    // 陆地
    auto landRItem = std::make_unique<RenderItem>();
    landRItem->texTransform = Math::Transpose(Math::Matrix4(Math::AffineTransform(Math::Matrix3::MakeScale(5.0f, 5.0f, 1.0f))));
    landRItem->IndexCount = m_mapGeometries["landGeo"]->geoMap["land"].IndexCount;
    landRItem->StartIndexLocation = m_mapGeometries["landGeo"]->geoMap["land"].StartIndexLocation;
    landRItem->BaseVertexLocation = m_mapGeometries["landGeo"]->geoMap["land"].BaseVertexLocation;
    landRItem->geo = m_mapGeometries["landGeo"].get();
    landRItem->mat = m_mapMaterial["grass"].get();
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(landRItem.get());

    // 箱子
    auto boxRItem = std::make_unique<RenderItem>();
    boxRItem->modeToWorld = Math::Transpose(Math::Matrix4(Math::AffineTransform(Math::Vector3{ 3.0f, 2.0f, -9.0f })));
    boxRItem->IndexCount = m_mapGeometries["boxGeo"]->geoMap["box"].IndexCount;
    boxRItem->StartIndexLocation = m_mapGeometries["boxGeo"]->geoMap["box"].StartIndexLocation;
    boxRItem->BaseVertexLocation = m_mapGeometries["boxGeo"]->geoMap["box"].BaseVertexLocation;
    boxRItem->geo = m_mapGeometries["boxGeo"].get();
    boxRItem->mat = m_mapMaterial["wireFence"].get();
    m_vecRenderItems[(int)RenderLayer::AlphaTest].push_back(boxRItem.get());

    // 海水
    auto waterRItem = std::make_unique<RenderItem>();
    waterRItem->texTransform = Math::Transpose(Math::Matrix4(Math::AffineTransform(Math::Matrix3::MakeScale(5.0f, 5.0f, 1.0f))));
    waterRItem->IndexCount = m_mapGeometries["wavesGeo"]->geoMap["wave"].IndexCount;
    waterRItem->StartIndexLocation = m_mapGeometries["wavesGeo"]->geoMap["wave"].StartIndexLocation;
    waterRItem->BaseVertexLocation = m_mapGeometries["wavesGeo"]->geoMap["wave"].BaseVertexLocation;
    waterRItem->geo = m_mapGeometries["wavesGeo"].get();
    waterRItem->mat = m_mapMaterial["water"].get();
    m_pWaveRItem = waterRItem.get();
    m_vecRenderItems[(int)RenderLayer::Transparent].push_back(waterRItem.get());

    auto treeSpritesRitem = std::make_unique<RenderItem>();
    treeSpritesRitem->IndexCount = m_mapGeometries["treeSpritesGeo"]->geoMap["points"].IndexCount;
    treeSpritesRitem->StartIndexLocation = m_mapGeometries["treeSpritesGeo"]->geoMap["points"].StartIndexLocation;
    treeSpritesRitem->BaseVertexLocation = m_mapGeometries["treeSpritesGeo"]->geoMap["points"].BaseVertexLocation;
    treeSpritesRitem->geo = m_mapGeometries["treeSpritesGeo"].get();
    treeSpritesRitem->mat = m_mapMaterial["treeSprites"].get();
    treeSpritesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    m_vecRenderItems[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

    m_vecAll.push_back(std::move(landRItem));
    m_vecAll.push_back(std::move(boxRItem));
    m_vecAll.push_back(std::move(waterRItem));
    m_vecAll.push_back(std::move(treeSpritesRitem));
}

float GameApp::GetHillsHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 GameApp::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

void GameApp::UpdateWaves(float deltaT)
{
    // Every quarter second, generate a random wave.
    static float t_base = 0.0f;
    if (t_base >= 0.25f)
    {
        t_base -= 0.25f;


        int i = Rand(4, m_waves.RowCount() - 5);
        int j = Rand(4, m_waves.ColumnCount() - 5);

        float r = RandF(0.2f, 0.5f);

        m_waves.Disturb(i, j, r);
    }
    else
    {
        t_base += deltaT;
    }

    // Update the wave simulation.
    m_waves.Update(deltaT);

    // Update the wave vertex buffer with the new solution.
    for (int i = 0; i < m_waves.VertexCount(); ++i)
    {
        auto p = &m_pWavesVec->at(i);

        p->Pos = m_waves.Position(i);
        p->Normal = m_waves.Normal(i);

        // Derive tex-coords from position by 
        // mapping [-w/2,w/2] --> [0,1]
        p->TexC.x = 0.5f + p->Pos.x / m_waves.Width();
        p->TexC.y = 0.5f - p->Pos.z / m_waves.Depth();
    }

    AnimateMaterials(deltaT);
}

void GameApp::AnimateMaterials(float deltaT)
{
    // Scroll the water material texture coordinates.
    float tu = (float)m_pWaveRItem->matTransform.GetW().GetX();
    float tv = (float)m_pWaveRItem->matTransform.GetW().GetY();

    tu += 0.1f * deltaT;
    tv += 0.02f * deltaT;

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    m_pWaveRItem->matTransform.SetW({ tu, tv, 0.0f, 1.0f });
}