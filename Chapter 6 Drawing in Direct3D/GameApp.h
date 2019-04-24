#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"

class RootSignature;
class StructuredBuffer;
class ByteAddressBuffer;
class GraphicsPSO;
class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:
    // 根签名
    RootSignature m_RootSignature;
    // 顶点缓冲区
    StructuredBuffer m_VertexBuffer;
    // 索引缓冲区
    ByteAddressBuffer m_IndexBuffer;
    // 流水线对象
    GraphicsPSO m_PSO;
};