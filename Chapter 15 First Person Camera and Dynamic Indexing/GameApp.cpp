#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "TextureManager.h"
#include "GameInput.h"

#include "GeometryGenerator.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"

void GameApp::Startup(void)
{
    buildPSO();
    buildGeo();
    buildMaterials();
    buildRenderItem();
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
    psc.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    psc.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    psc.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    psc.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    psc.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    psc.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    psc.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
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
        obc.texTransform = item->texTransform;
        obc.matTransform = item->matTransform;
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        gfxContext.SetDynamicDescriptor(3, 0, item->mat->srv);

        MaterialConstants mc;
        mc.DiffuseAlbedo = item->mat->diffuseAlbedo;
        mc.FresnelR0 = item->mat->fresnelR0;
        mc.Roughness = item->mat->roughness;
        gfxContext.SetDynamicConstantBufferView(2, sizeof(mc), &mc);

        gfxContext.DrawIndexed(item->IndexCount, item->StartIndexLocation, item->BaseVertexLocation);
    }
}

void GameApp::buildPSO()
{
    // 创建根签名
    m_RootSignature.Reset(4, 1);
    m_RootSignature.InitStaticSampler(0, Graphics::SamplerAnisoWrapDesc);
    m_RootSignature[0].InitAsConstantBuffer(0);
    m_RootSignature[1].InitAsConstantBuffer(1);
    m_RootSignature[2].InitAsConstantBuffer(2);
    m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    m_RootSignature.Finalize(L"15 RS", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
    defaultPSO.SetVertexShader(g_pdefaultVS, sizeof(g_pdefaultVS));
    defaultPSO.SetPixelShader(g_pdefaultPS, sizeof(g_pdefaultPS));
    defaultPSO.Finalize();

    // 默认PSO
    m_mapPSO[E_EPT_DEFAULT] = defaultPSO;
}

void GameApp::buildGeo()
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

inline void GameApp::makeMaterials(const std::string& name, const Math::Vector4& diffuseAlbedo, const Math::Vector3& fresnelR0,
    const float roughness, const std::string& materialName)
{
    std::wstring strFile(materialName.begin(), materialName.end());
    auto item = std::make_unique<Material>(); 
    item->name = name;
    item->diffuseAlbedo = diffuseAlbedo;
    item->fresnelR0 = fresnelR0;
    item->roughness = roughness;
    item->srv = TextureManager::LoadFromFile(strFile, true)->GetSRV();
    m_mapMaterial[name] = std::move(item);
}

void GameApp::buildMaterials()
{
    TextureManager::Initialize(L"Textures/");
    makeMaterials("brick", { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.02f, 0.02f, 0.02f }, 0.1f, "bricks");
    makeMaterials("stone", { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.3f, "stone");
    makeMaterials("tile", { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.02f, 0.02f, 0.02f }, 0.3f, "tile");
    makeMaterials("crate", { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.05f, 0.05f, 0.05f }, 0.2f, "WoodCrate01");
}

void GameApp::buildRenderItem()
{
    using namespace Math;
    auto boxRitem = std::make_unique<RenderItem>();
    boxRitem->modeToWorld = Transpose(Matrix4(AffineTransform(Matrix3::MakeScale(2.0f, 2.0f, 2.0f), Vector3(0.0f, 1.0f, 0.0f))));
    boxRitem->texTransform = Transpose(Matrix4(kIdentity));
    boxRitem->matTransform = Transpose(Matrix4(kIdentity));
    boxRitem->mat = m_mapMaterial["crate"].get();
    boxRitem->geo = m_mapGeometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->geo->geoMap["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->geo->geoMap["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->geo->geoMap["box"].BaseVertexLocation;
    m_vecRenderItems[(int)RenderLayer::Opaque].push_back(boxRitem.get());
    m_vecAll.push_back(std::move(boxRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->modeToWorld = Transpose(Matrix4(kIdentity));
    gridRitem->texTransform = Transpose(Matrix4::MakeScale({ 8.0f, 8.0f, 1.0f }));
    gridRitem->matTransform = Transpose(Matrix4(kIdentity));
    gridRitem->mat = m_mapMaterial["tile"].get();
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
        leftCylRitem->mat = m_mapMaterial["brick"].get();
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
        rightCylRitem->mat = m_mapMaterial["brick"].get();
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
        leftSphereRitem->mat = m_mapMaterial["stone"].get();
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
        rightSphereRitem->mat = m_mapMaterial["stone"].get();
        rightSphereRitem->geo = m_mapGeometries["shapeGeo"].get();
        rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRitem->IndexCount = rightSphereRitem->geo->geoMap["sphere"].IndexCount;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->geo->geoMap["sphere"].StartIndexLocation;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->geo->geoMap["sphere"].BaseVertexLocation;
        m_vecRenderItems[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());
        m_vecAll.push_back(std::move(rightSphereRitem));
    }
}