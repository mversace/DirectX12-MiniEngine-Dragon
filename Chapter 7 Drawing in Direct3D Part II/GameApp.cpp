#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "GeometryGenerator.h"

#include <array>
#include <DirectXColors.h>
#include "GameInput.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"

namespace GameCore
{
    extern HWND g_hWnd;
}

using namespace Graphics;
void GameApp::Startup(void)
{
    buildShapesData();
}

void GameApp::Cleanup(void)
{
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();
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
    

    float x = m_radius * cosf(m_yRotate) * sinf(m_xRotate);
    float y = m_radius * sinf(m_yRotate);
    float z = m_radius * cosf(m_yRotate) * cosf(m_xRotate);

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
}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    gfxContext.ClearColor(g_SceneColorBuffer);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ, true);
    gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());

    renderShapes(gfxContext);

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}

void GameApp::buildShapesData()
{
    buildShapesVI();

    // 根签名
    m_RootSignature.Reset(1, 0);
    m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSignature.Finalize(L"box signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

    m_PSO.SetRootSignature(m_RootSignature);
    auto raster = RasterizerDefault;
    raster.FillMode = D3D12_FILL_MODE_WIREFRAME;
    m_PSO.SetRasterizerState(raster);
    m_PSO.SetBlendState(BlendDisable);
    m_PSO.SetDepthStencilState(DepthStateDisabled);
    m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    m_PSO.SetVertexShader(g_pdefaultVS, sizeof(g_pdefaultVS));
    m_PSO.SetPixelShader(g_pdefaultPS, sizeof(g_pdefaultPS));
    m_PSO.Finalize();
}

void GameApp::buildShapesVI()
{
    // 顶点结构体
    struct Vertex
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Color;
    };

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
        vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
    }

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
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
    item.modelToWorld = Matrix4(AffineTransform(Matrix3::MakeScale(2.0f, 2.0f, 2.0f), Vector3(0.0f, 0.5f, 0.0f)));
    item.indexCount = (UINT)box.Indices32.size();
    item.startIndex = boxIndexOffset;
    item.baseVertex = boxVertexOffset;
    m_vecShapes.push_back(item);

    // grid
    item.modelToWorld = Matrix4(kIdentity);
    item.indexCount = (UINT)grid.Indices32.size();
    item.startIndex = gridIndexOffset;
    item.baseVertex = gridVertexOffset;
    m_vecShapes.push_back(item);

    for (int i = 0; i < 5; ++i)
    {
        Matrix4 leftCylWorld = Matrix4(AffineTransform(Vector3(-5.0f, 1.5f, -10.0f + i * 5.0f)));
        Matrix4 rightCylWorld = Matrix4(AffineTransform(Vector3(+5.0f, 1.5f, -10.0f + i * 5.0f)));

        Matrix4 leftSphereWorld = Matrix4(AffineTransform(Vector3(-5.0f, 3.5f, -10.0f + i * 5.0f)));
        Matrix4 rightSphereWorld = Matrix4(AffineTransform(Vector3(+5.0f, 3.5f, -10.0f + i * 5.0f)));

        // cylinder
        item.indexCount = (UINT)cylinder.Indices32.size();
        item.startIndex = cylinderIndexOffset;
        item.baseVertex = cylinderVertexOffset;
        item.modelToWorld = leftCylWorld;
        m_vecShapes.push_back(item);
        item.modelToWorld = rightCylWorld;
        m_vecShapes.push_back(item);

        // sphere
        item.indexCount = (UINT)sphere.Indices32.size();
        item.startIndex = sphereIndexOffset;
        item.baseVertex = sphereVertexOffset;
        item.modelToWorld = leftSphereWorld;
        m_vecShapes.push_back(item);
        item.modelToWorld = rightSphereWorld;
        m_vecShapes.push_back(item);
    }
}

void GameApp::renderShapes(GraphicsContext& gfxContext)
{
    // 设置渲染流水线
    gfxContext.SetPipelineState(m_PSO);
    // 设置根签名
    gfxContext.SetRootSignature(m_RootSignature);
    // 设置顶点拓扑结构
    gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 设置顶点视图
    gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
    // 设置索引视图
    gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

    for (auto& item : m_vecShapes)
    {
        Matrix4 a = m_ViewProjMatrix * item.modelToWorld;
        // 设置常量缓冲区数据
        gfxContext.SetDynamicConstantBufferView(0, sizeof(a), &a);
        // 绘制
        gfxContext.DrawIndexedInstanced(item.indexCount, 1, item.startIndex, item.baseVertex, 0);
    }
}