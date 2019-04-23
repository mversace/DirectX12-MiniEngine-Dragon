#include "GameApp.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"

using namespace Graphics;
void GameApp::Startup(void)
{
	
}

void GameApp::Cleanup(void)
{
	
}

void GameApp::Update(float deltaT)
{
	
}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);

    g_DisplayPlane[g_CurrentBuffer].SetClearColor({ 0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f });
    gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
    gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());

    gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

    gfxContext.Finish();
}