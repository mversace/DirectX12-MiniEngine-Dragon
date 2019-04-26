#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"

#include <array>
#include <DirectXColors.h>
#include "GameInput.h"
#include "CompiledShaders/defaultVS.h"
#include "CompiledShaders/defaultPS.h"

using namespace Graphics;
void GameApp::Startup(void)
{
    m_RootSignature.Reset(1, 0);
    m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSignature.Finalize(L"box signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    struct Vertex
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Color;
    };

    std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Blue) }),
        Vertex({ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

    // GPUBuff类，自动把对象通过上传缓冲区传到了对应的默认堆中
    m_VertexBuffer.Create(L"vertex buff", 8, sizeof(Vertex), vertices.data());
    m_IndexBuffer.Create(L"index buff", 36, sizeof(std::uint16_t), indices.data());

    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

    m_PSO.SetRootSignature(m_RootSignature);
    m_PSO.SetRasterizerState(RasterizerDefault);
    m_PSO.SetBlendState(BlendDisable);
    m_PSO.SetDepthStencilState(DepthStateDisabled);
    m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    m_PSO.SetVertexShader(g_pdefaultVS, sizeof(g_pdefaultVS));
    m_PSO.SetPixelShader(g_pdefaultPS, sizeof(g_pdefaultPS));
    m_PSO.Finalize();
}

void GameApp::Cleanup(void)
{
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();
}

void GameApp::Update(float deltaT)
{
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
    // 设置常量缓冲区数据
    gfxContext.SetDynamicConstantBufferView(0, sizeof(m_ViewProjMatrix), &m_ViewProjMatrix);
    // 绘制
    gfxContext.DrawIndexedInstanced(36, 1, 0, 0, 0);

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}