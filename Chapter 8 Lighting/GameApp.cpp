#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "GeometryGenerator.h"

#include <DirectXColors.h>
#include <fstream>
#include "GameInput.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"

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

using namespace Graphics;
void GameApp::Startup(void)
{
    buildShapesData();
    buildSkull();
    buildLandAndWaves();

    // 根签名
    m_RootSignature.Reset(3, 0);
    m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);
    m_RootSignature[2].InitAsConstantBuffer(2, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSignature.Finalize(L"box signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

    m_PSO.SetRootSignature(m_RootSignature);
    auto raster = RasterizerDefault;
    raster.FillMode = D3D12_FILL_MODE_WIREFRAME;
    m_PSO.SetRasterizerState(raster);
    m_PSO.SetBlendState(BlendDisable);
    m_PSO.SetDepthStencilState(DepthStateReadWrite);
    m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    m_PSO.SetVertexShader(g_pdefaultVS, sizeof(g_pdefaultVS));
    m_PSO.SetPixelShader(g_pdefaultPS, sizeof(g_pdefaultPS));
    m_PSO.Finalize();

    m_PSOEx = m_PSO;
    m_PSOEx.SetRasterizerState(RasterizerDefault);
    m_PSOEx.Finalize();
}

void GameApp::Cleanup(void)
{
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();
    m_VertexBufferSkull.Destroy();
    m_IndexBufferSkull.Destroy();

    m_VertexBufferLand.Destroy();
    m_IndexBufferLand.Destroy();
    m_IndexBufferWaves.Destroy();
}

void GameApp::Update(float deltaT)
{
    float fps = GetFrameRate(); // fps = frameCnt / 1
    float mspf = GetFrameTime();

    std::wstring fpsStr = std::to_wstring(fps);
    std::wstring mspfStr = std::to_wstring(mspf);

    std::wstring windowText = L"CrossGate";
    windowText +=
        L"    fps: " + fpsStr +
        L"   mspf: " + mspfStr;

    SetWindowText(GameCore::g_hWnd, windowText.c_str());

    if (GameInput::IsFirstPressed(GameInput::kKey_f1))
        m_bRenderShapes = !m_bRenderShapes;
    if (GameInput::IsFirstPressed(GameInput::kKey_f2))
    {
        m_bRenderFill = !m_bRenderFill;
    }

    if (GameInput::IsPressed(GameInput::kMouse0) || GameInput::IsPressed(GameInput::kMouse1)) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = GameInput::GetAnalogInput(GameInput::kAnalogMouseX) - m_xLast;
        float dy = GameInput::GetAnalogInput(GameInput::kAnalogMouseY) - m_yLast;

        if (GameInput::IsPressed(GameInput::kMouse0))
        {
            // Update angles based on input to orbit camera around box.
            m_xRotate += (dx - m_xDiff);
            m_yRotate += (dy - m_yDiff);
            m_yRotate = (std::max)(-XM_PIDIV2 + 0.1f, m_yRotate);
            m_yRotate = (std::min)(XM_PIDIV2 - 0.1f, m_yRotate);
        }
        else
        {
            m_radius += dx - dy - (m_xDiff - m_yDiff);
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

    // 滚轮消息
    if (float fl = GameInput::GetAnalogInput(GameInput::kAnalogMouseScroll))
    {
        if (fl > 0)
            m_radius -= 5;
        else
            m_radius += 5;
    }

    float x = m_radius* cosf(m_yRotate)* sinf(m_xRotate);
    float y = m_radius* sinf(m_yRotate);
    float z = m_radius* cosf(m_yRotate)* cosf(m_xRotate);

    m_Camera.SetEyeAtUp({ x, y, z }, Vector3(kZero), Vector3(kYUnitVector));
    m_Camera.Update();

    m_ViewProjMatrix = m_Camera.GetViewProjMatrix();

    m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
    m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();

    if (!m_bRenderShapes)
        UpdateWaves(deltaT);
}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    gfxContext.ClearColor(g_SceneColorBuffer);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());

    // 设置渲染流水线
    if (m_bRenderFill)
        gfxContext.SetPipelineState(m_PSOEx);
    else
        gfxContext.SetPipelineState(m_PSO);

    // 设置根签名
    gfxContext.SetRootSignature(m_RootSignature);
    // 设置顶点拓扑结构
    gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 开始绘制
    if (m_bRenderShapes)
        renderShapes(gfxContext);
    else
        renderLandAndWaves(gfxContext);

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::buildShapesData()
{
    // 创建形状顶点
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

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
    }

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    // GPUBuff类，自动把对象通过上传缓冲区传到了对应的默认堆中
    m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    m_IndexBuffer.Create(L"index buff", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());


    // 保存物体对应的索引数量、位置、顶点位置、模型转世界的矩阵
    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    renderItem item;
    // box
    item.modelToWorld = Transpose(Matrix4(AffineTransform(Matrix3::MakeScale(2.0f, 2.0f, 2.0f), Vector3(0.0f, 0.5f, 0.0f))));
    item.indexCount = (UINT)box.Indices32.size();
    item.startIndex = boxIndexOffset;
    item.baseVertex = boxVertexOffset;
    item.diffuseAlbedo = XMFLOAT4(Colors::LightSteelBlue);
    item.fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    item.roughness = 0.3f;
    m_vecShapes.push_back(item);

    // grid
    item.modelToWorld = Transpose(Matrix4(kIdentity));
    item.indexCount = (UINT)grid.Indices32.size();
    item.startIndex = gridIndexOffset;
    item.baseVertex = gridVertexOffset;
    item.diffuseAlbedo = XMFLOAT4(Colors::LightGray);
    item.fresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    item.roughness = 0.2f;
    m_vecShapes.push_back(item);

    for (int i = 0; i < 5; ++i)
    {
        Matrix4 leftCylWorld = Transpose(Matrix4(AffineTransform(Vector3(-5.0f, 1.5f, -10.0f + i * 5.0f))));
        Matrix4 rightCylWorld = Transpose(Matrix4(AffineTransform(Vector3(+5.0f, 1.5f, -10.0f + i * 5.0f))));

        Matrix4 leftSphereWorld = Transpose(Matrix4(AffineTransform(Vector3(-5.0f, 3.5f, -10.0f + i * 5.0f))));
        Matrix4 rightSphereWorld = Transpose(Matrix4(AffineTransform(Vector3(+5.0f, 3.5f, -10.0f + i * 5.0f))));

        // cylinder
        item.indexCount = (UINT)cylinder.Indices32.size();
        item.startIndex = cylinderIndexOffset;
        item.baseVertex = cylinderVertexOffset;
        item.modelToWorld = leftCylWorld;
        item.diffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
        item.fresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
        item.roughness = 0.1f;
        m_vecShapes.push_back(item);
        item.modelToWorld = rightCylWorld;
        m_vecShapes.push_back(item);

        // sphere
        item.indexCount = (UINT)sphere.Indices32.size();
        item.startIndex = sphereIndexOffset;
        item.baseVertex = sphereVertexOffset;
        item.modelToWorld = leftSphereWorld;
        item.diffuseAlbedo = XMFLOAT4(Colors::LightSteelBlue);
        item.fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
        item.roughness = 0.3f;
        m_vecShapes.push_back(item);
        item.modelToWorld = rightSphereWorld;
        m_vecShapes.push_back(item);
    }
}

void GameApp::buildSkull()
{
    // create skull
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

    // GPUBuff类，自动把对象通过上传缓冲区传到了对应的默认堆中
    m_VertexBufferSkull.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    m_IndexBufferSkull.Create(L"index buff", (UINT)indices.size(), sizeof(std::int32_t), indices.data());

    m_skull.modelToWorld = Transpose(Matrix4(AffineTransform(Matrix3::MakeScale(0.5f, 0.5f, 0.5f), Vector3(0.0f, 1.0f, 0.0f))));
    m_skull.indexCount = (UINT)indices.size();
    m_skull.startIndex = 0;
    m_skull.baseVertex = 0;
    m_skull.diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    m_skull.fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    m_skull.roughness = 0.3f;
}

void GameApp::renderShapes(GraphicsContext& gfxContext)
{
    PassConstants psc;
    // https://www.cnblogs.com/X-Jun/p/9808727.html
    // C++代码端进行转置，HLSL中使用matrix(列矩阵)
    // mul函数让向量放在左边(行向量)，实际运算是(行向量 X 行矩阵) 然后行矩阵为了使用dp4运算发生了转置成了列矩阵
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

    // 设置顶点视图
    gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
    // 设置索引视图
    gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

    for (auto& item : m_vecShapes)
    {
        // 设置常量缓冲区数据
        ObjectConstants obc;
        obc.World = item.modelToWorld;
        gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

        MaterialConstants mc;
        mc.DiffuseAlbedo = { item.diffuseAlbedo.x, item.diffuseAlbedo.y, item.diffuseAlbedo.z, item.diffuseAlbedo.w };
        mc.FresnelR0 = item.fresnelR0;
        mc.Roughness = item.roughness;
        gfxContext.SetDynamicConstantBufferView(2, sizeof(mc), &mc);

        // 绘制
        gfxContext.DrawIndexedInstanced(item.indexCount, 1, item.startIndex, item.baseVertex, 0);
    }

    // 绘制skull
    gfxContext.SetVertexBuffer(0, m_VertexBufferSkull.VertexBufferView());
    gfxContext.SetIndexBuffer(m_IndexBufferSkull.IndexBufferView());

    ObjectConstants obc;
    obc.World = m_skull.modelToWorld;
    gfxContext.SetDynamicConstantBufferView(0, sizeof(obc), &obc);

    MaterialConstants mc;
    mc.DiffuseAlbedo = { m_skull.diffuseAlbedo.x, m_skull.diffuseAlbedo.y, m_skull.diffuseAlbedo.z, m_skull.diffuseAlbedo.w };
    mc.FresnelR0 = m_skull.fresnelR0;
    mc.Roughness = m_skull.roughness;
    gfxContext.SetDynamicConstantBufferView(2, sizeof(mc), &mc);

    // 绘制
    gfxContext.DrawIndexedInstanced(m_skull.indexCount, 1, m_skull.startIndex, m_skull.baseVertex, 0);
}

void GameApp::buildLandAndWaves()
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
    }

    std::vector<std::uint16_t> indices = grid.GetIndices16();

    m_VertexBufferLand.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
    m_IndexBufferLand.Create(L"index buff", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());


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
    m_IndexBufferWaves.Create(L"wave index buff", (UINT)waveIndices.size(), sizeof(std::uint16_t), waveIndices.data());

}

float GameApp::GetHillsHeight(float x, float z) const
{
    return 0.3f* (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 GameApp::GetHillsNormal(float x, float z)const
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

void GameApp::renderLandAndWaves(GraphicsContext& gfxContext)
{
    // land
    // 设置顶点视图
    gfxContext.SetVertexBuffer(0, m_VertexBufferLand.VertexBufferView());
    // 设置索引视图
    gfxContext.SetIndexBuffer(m_IndexBufferLand.IndexBufferView());

    // 绘制
    gfxContext.DrawIndexedInstanced(m_IndexBufferLand.GetElementCount(), 1, 0, 0, 0);

    // waves
    gfxContext.SetIndexBuffer(m_IndexBufferWaves.IndexBufferView());

    gfxContext.SetDynamicVB(0, m_verticesWaves.size(), sizeof(Vertex), m_verticesWaves.data());

    // 绘制
    gfxContext.DrawIndexedInstanced(m_IndexBufferWaves.GetElementCount(), 1, 0, 0, 0);
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
    m_verticesWaves.clear();
    for (int i = 0; i < m_waves.VertexCount(); ++i)
    {
        Vertex v;

        v.Pos = m_waves.Position(i);
        v.Normal = m_waves.Normal(i);

        m_verticesWaves.push_back(v);
    }
}