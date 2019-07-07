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
#include <DirectXCollision.h>

#include "CompiledShaders/dynamicIndexDefaultPS.h"
#include "CompiledShaders/dynamicIndexDefaultVS.h"
#include "CompiledShaders/skyboxPS.h"
#include "CompiledShaders/skyboxVS.h"

void GameApp::Startup(void)
{
    buildPSO();
    buildGeo();
    buildMaterials();
    buildRenderItem();

    m_Camera.SetEyeAtUp({ 0.0f, 5.0f, -10.0f }, { 0.0f, 0.0f, 0.0f }, Math::Vector3(Math::kYUnitVector));
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

    // skull 的世界坐标一直变化
    static float fAllTime = 0;
    fAllTime += deltaT;
    using namespace Math;
    Matrix4 skullScale = Matrix4::MakeScale(0.2f);
    Matrix4 skullOffset = Matrix4(Matrix3(kIdentity), { 3.0f, 2.0f, 0.0f });
    Matrix4 skullLocalRotate = Matrix3::MakeYRotation(2.0f * fAllTime);
    Matrix4 skullGlobalRotate = Matrix3::MakeYRotation(0.5f * fAllTime);
    // 注意反向
    m_SkullRItem->modeToWorld = Transpose(skullGlobalRotate * skullOffset * skullLocalRotate * skullScale);

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
    psc.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    psc.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    psc.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    psc.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    psc.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    psc.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    psc.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
    gfxContext.SetDynamicConstantBufferView(1, sizeof(psc), &psc);

    // 设置全部的纹理参数
    gfxContext.SetBufferSRV(2, m_mats);

    // 设置全部的纹理资源
    gfxContext.SetDynamicDescriptors(3, 0, 4, &m_srvs[0]);

    gfxContext.SetPipelineState(m_mapPSO[E_EPT_DEFAULT]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Opaque]);

    // 绘制天空盒
    gfxContext.SetPipelineState(m_mapPSO[E_EPT_SKY]);
    drawRenderItems(gfxContext, m_vecRenderItems[(int)RenderLayer::Sky]);

    gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::RenderUI(class GraphicsContext& gfxContext)
{
    
}

void GameApp::drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem *>& ritems)
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
        obc.MaterialIndex = item->MaterialIndex;
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        gfxContext.DrawIndexed(item->IndexCount, item->StartIndexLocation, item->BaseVertexLocation);
    }
}

void GameApp::buildPSO()
{
    // 创建根签名
    m_RootSignature.Reset(4, 1);
    m_RootSignature.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc);
    m_RootSignature[0].InitAsConstantBuffer(0);
    m_RootSignature[1].InitAsConstantBuffer(1);
    m_RootSignature[2].InitAsBufferSRV(0);
    m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    m_RootSignature.Finalize(L"18 RS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

    // 天空盒PSO
    auto ras = Graphics::RasterizerDefaultCw;
    ras.CullMode = D3D12_CULL_MODE_NONE;
    auto dep = Graphics::DepthStateReadWrite;
    dep.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    GraphicsPSO skyPSO = defaultPSO;
    skyPSO.SetRasterizerState(ras);
    skyPSO.SetDepthStencilState(dep);
    skyPSO.SetVertexShader(g_pskyboxVS, sizeof(g_pskyboxVS));
    skyPSO.SetPixelShader(g_pskyboxPS, sizeof(g_pskyboxPS));
    skyPSO.Finalize();
    m_mapPSO[E_EPT_SKY] = skyPSO;
}

void GameApp::buildGeo()
{
    buildShapeGeo();
    buildSkullGeo();
}

void GameApp::buildShapeGeo()
{
    // 创建形状顶点
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

    auto totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
    }

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].TexC = grid.Vertices[i].TexC;
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].TexC = sphere.Vertices[i].TexC;
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].TexC = cylinder.Vertices[i].TexC;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "shapeGeo";

    // GPUBuff类，自动把对象通过上传缓冲区传到了对应的默认堆中
    geo->createVertex(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    geo->createIndex(L"index buff", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

    geo->geoMap["box"] = boxSubmesh;
    geo->geoMap["grid"] = gridSubmesh;
    geo->geoMap["sphere"] = sphereSubmesh;
    geo->geoMap["cylinder"] = cylinderSubmesh;

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
    geo->storeVertexAndIndex(vertices, indices);

    SubmeshGeometry submesh;
    submesh.IndexCount = 3 * tcount;
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->geoMap["skull"] = submesh;

    m_mapGeometries[geo->name] = std::move(geo);
}

void GameApp::buildMaterials()
{
    // 5个纹理材质
    std::vector<MaterialConstants> v = {
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.10f, 0.10f, 0.10f }, 0.3f, 0 },   // bricks
        { { 0.9f, 0.9f, 0.9f, 1.0f }, { 0.20f, 0.20f, 0.20f }, 0.1f, 1 },   // tile
        { { 0.0f, 0.0f, 0.1f, 1.0f }, { 0.98f, 0.97f, 0.95f }, 0.1f, 2 },   // mirror
        { { 0.8f, 0.8f, 0.8f, 1.0f }, { 0.20f, 0.20f, 0.20f }, 0.2f, 2 },   // skull
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.10f, 0.10f, 0.10f }, 1.0f, 3 },   // sky
    };

    // 存入所有纹理属性
    m_mats.Create(L"skull mats", (UINT)v.size(), sizeof(MaterialConstants), v.data());

    // 4个纹理
    m_srvs.resize(4);
    TextureManager::Initialize(L"Textures/");
    m_srvs[0] = TextureManager::LoadFromFile(L"bricks2", true)->GetSRV();
    m_srvs[1] = TextureManager::LoadFromFile(L"tile", true)->GetSRV();
    m_srvs[2] = TextureManager::LoadFromFile(L"white1x1", true)->GetSRV();
    m_srvs[3] = TextureManager::LoadFromFile(L"grasscube1024", true)->GetSRV();
}

void GameApp::buildRenderItem()
{
    using namespace Math;
    auto skyRitem = std::make_unique<RenderItem>();
    skyRitem->modeToWorld = Transpose(Matrix4::MakeScale(5000.0f));
    skyRitem->texTransform = Transpose(Matrix4(kIdentity));
    skyRitem->matTransform = Transpose(Matrix4(kIdentity));
    skyRitem->MaterialIndex = 4;
    skyRitem->geo = m_mapGeometries["shapeGeo"].get();
    skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount = skyRitem->geo->geoMap["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->geo->geoMap["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->geo->geoMap["sphere"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Sky].push_back(skyRitem.get());
    m_vecAll.push_back(std::move(skyRitem));

    auto boxRitem = std::make_unique<RenderItem>();
    boxRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Matrix3::MakeScale(2.0f, 1.0f, 2.0f), Vector3(0.0f, 0.5f, 0.0f))));
    boxRitem->texTransform = Transpose(Matrix4(kIdentity));
    boxRitem->matTransform = Transpose(Matrix4(kIdentity));
    boxRitem->MaterialIndex = 0;
    boxRitem->geo = m_mapGeometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->geo->geoMap["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->geo->geoMap["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->geo->geoMap["box"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(boxRitem.get());
    m_vecAll.push_back(std::move(boxRitem));

    auto globeRitem = std::make_unique<RenderItem>();
    globeRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Matrix3::MakeScale(2.0f, 2.0f, 2.0f), Vector3(0.0f, 2.0f, 0.0f))));
    globeRitem->texTransform = Transpose(Matrix4::MakeScale(1.0f));
    globeRitem->matTransform = Transpose(Matrix4(kIdentity));
    globeRitem->MaterialIndex = 2;
    globeRitem->geo = m_mapGeometries["shapeGeo"].get();
    globeRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    globeRitem->IndexCount = globeRitem->geo->geoMap["sphere"].IndexCount;
    globeRitem->StartIndexLocation = globeRitem->geo->geoMap["sphere"].StartIndexLocation;
    globeRitem->BaseVertexLocation = globeRitem->geo->geoMap["sphere"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(globeRitem.get());
    m_vecAll.push_back(std::move(globeRitem));

    auto skullRitem = std::make_unique<RenderItem>();
    skullRitem->modeToWorld = Transpose(Matrix4(kIdentity));
    skullRitem->texTransform = Transpose(Matrix4(kIdentity));
    skullRitem->matTransform = Transpose(Matrix4(kIdentity));
    skullRitem->MaterialIndex = 3;
    skullRitem->geo = m_mapGeometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->IndexCount = skullRitem->geo->geoMap["skull"].IndexCount;
    skullRitem->StartIndexLocation = skullRitem->geo->geoMap["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->geo->geoMap["skull"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(skullRitem.get());
    m_SkullRItem = skullRitem.get();
    m_vecAll.push_back(std::move(skullRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->modeToWorld = Transpose(Matrix4(kIdentity));
    gridRitem->texTransform = Transpose(Matrix4::MakeScale({ 8.0f, 8.0f, 1.0f }));
    gridRitem->matTransform = Transpose(Matrix4(kIdentity));
    gridRitem->MaterialIndex = 1;
    gridRitem->geo = m_mapGeometries["shapeGeo"].get();
    gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->geo->geoMap["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->geo->geoMap["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->geo->geoMap["grid"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(gridRitem.get());
    m_vecAll.push_back(std::move(gridRitem));

    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRitem = std::make_unique<RenderItem>();
        leftCylRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Vector3(-5.0f, 1.5f, -10.0f + i * 5.0f))));
        leftCylRitem->texTransform = Transpose(Matrix4(kIdentity));
        leftCylRitem->matTransform = Transpose(Matrix4(kIdentity));
        leftCylRitem->MaterialIndex = 0;
        leftCylRitem->geo = m_mapGeometries["shapeGeo"].get();
        leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylRitem->IndexCount = leftCylRitem->geo->geoMap["cylinder"].IndexCount;
        leftCylRitem->StartIndexLocation = leftCylRitem->geo->geoMap["cylinder"].StartIndexLocation;
        leftCylRitem->BaseVertexLocation = leftCylRitem->geo->geoMap["cylinder"].BaseVertexLocation;
        m_vecRenderItems[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
        m_vecAll.push_back(std::move(leftCylRitem));

        auto rightCylRitem = std::make_unique<RenderItem>();
        rightCylRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Vector3(+5.0f, 1.5f, -10.0f + i * 5.0f))));
        rightCylRitem->texTransform = Transpose(Matrix4(kIdentity));
        rightCylRitem->matTransform = Transpose(Matrix4(kIdentity));
        rightCylRitem->MaterialIndex = 0;
        rightCylRitem->geo = m_mapGeometries["shapeGeo"].get();
        rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylRitem->IndexCount = rightCylRitem->geo->geoMap["cylinder"].IndexCount;
        rightCylRitem->StartIndexLocation = rightCylRitem->geo->geoMap["cylinder"].StartIndexLocation;
        rightCylRitem->BaseVertexLocation = rightCylRitem->geo->geoMap["cylinder"].BaseVertexLocation;
        m_vecRenderItems[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
        m_vecAll.push_back(std::move(rightCylRitem));

        auto leftSphereRitem = std::make_unique<RenderItem>();
        leftSphereRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Vector3(+5.0f, 3.5f, -10.0f + i * 5.0f))));
        leftSphereRitem->texTransform = Transpose(Matrix4(kIdentity));
        leftSphereRitem->matTransform = Transpose(Matrix4(kIdentity));
        leftSphereRitem->MaterialIndex = 2;
        leftSphereRitem->geo = m_mapGeometries["shapeGeo"].get();
        leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftSphereRitem->IndexCount = leftSphereRitem->geo->geoMap["sphere"].IndexCount;
        leftSphereRitem->StartIndexLocation = leftSphereRitem->geo->geoMap["sphere"].StartIndexLocation;
        leftSphereRitem->BaseVertexLocation = leftSphereRitem->geo->geoMap["sphere"].BaseVertexLocation;
        m_vecRenderItems[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
        m_vecAll.push_back(std::move(leftSphereRitem));

        auto rightSphereRitem = std::make_unique<RenderItem>();
        rightSphereRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Vector3(-5.0f, 3.5f, -10.0f + i * 5.0f))));
        rightSphereRitem->texTransform = Transpose(Matrix4(kIdentity));
        rightSphereRitem->matTransform = Transpose(Matrix4(kIdentity));
        rightSphereRitem->MaterialIndex = 2;
        rightSphereRitem->geo = m_mapGeometries["shapeGeo"].get();
        rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRitem->IndexCount = rightSphereRitem->geo->geoMap["sphere"].IndexCount;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->geo->geoMap["sphere"].StartIndexLocation;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->geo->geoMap["sphere"].BaseVertexLocation;
        m_vecRenderItems[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());
        m_vecAll.push_back(std::move(rightSphereRitem));
    }
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
