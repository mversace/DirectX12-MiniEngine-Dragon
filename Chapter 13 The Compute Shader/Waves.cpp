//***************************************************************************************
// Waves.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "Waves.h"
#include <ppl.h>
#include <algorithm>
#include <vector>
#include <cassert>
#include <fstream>
#include "CommandContext.h"
#include "ReadbackBuffer.h"

#include "CompiledShaders/waveDisturbCS.h"
#include "CompiledShaders/waveUpdateCS.h"

using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping)
{
    mNumRows = m;
    mNumCols = n;

    mVertexCount = m*n;
    mTriangleCount = (m - 1)*(n - 1) * 2;

    mTimeStep = dt;
    mSpatialStep = dx;

    float d = damping*dt + 2.0f;
    float e = (speed*speed)*(dt*dt) / (dx*dx);
    mK1 = (damping*dt - 2.0f) / d;
    mK2 = (4.0f - 8.0f*e) / d;
    mK3 = (2.0f*e) / d;
}

Waves::~Waves()
{
}

void Waves::Destory()
{
    _bufferDisturb.Destroy();
    _bufferPre.Destroy();
    _bufferWaves.Destroy();
}

int Waves::RowCount()const
{
	return mNumRows;
}

int Waves::ColumnCount()const
{
	return mNumCols;
}

int Waves::VertexCount()const
{
	return mVertexCount;
}

int Waves::TriangleCount()const
{
	return mTriangleCount;
}

float Waves::Width()const
{
	return mNumCols*mSpatialStep;
}

float Waves::Depth()const
{
	return mNumRows*mSpatialStep;
}

float Waves::SpatialStep()const
{
    return mSpatialStep;
}

void Waves::Update(float dt)
{
	static float t = 0;

	// Accumulate time.
	t += dt;

	// Only update the simulation at the specified time step.
	if( t >= mTimeStep )
	{
        ComputeContext& context = ComputeContext::Begin(L"wave update");
        context.SetRootSignature(_updateRS);
        context.SetPipelineState(_updatePSO);

        context.SetConstants(0, mK1, mK2, mK3);
        context.TransitionResource(_bufferPre, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(1, 0, _bufferPre.GetUAV());
        context.TransitionResource(_bufferDisturb, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(2, 0, _bufferDisturb.GetUAV());
        context.TransitionResource(_bufferWaves, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(3, 0, _bufferWaves.GetUAV());

        context.Dispatch(mNumCols / 16, mNumRows / 16, 1);

        context.Finish(true);

        auto buffer = _bufferPre;
        _bufferPre = _bufferDisturb;
        _bufferDisturb = _bufferWaves;
        _bufferWaves = buffer;

        t = 0.0f;
	}
}

void Waves::Disturb(int i, int j, float magnitude)
{
    ComputeContext& context = ComputeContext::Begin(L"wave disturb");
    context.SetRootSignature(_disturbRS);
    context.SetPipelineState(_disturbPSO);
    // i=row, j=col，所以这里要反过来
    context.SetConstants(0, magnitude, j, i);

    context.TransitionResource(_bufferDisturb, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.SetDynamicDescriptor(1, 0, _bufferDisturb.GetUAV());

    context.Dispatch(1, 1, 1);
    context.Finish(true);
}

ColorBuffer& Waves::getWavesBuffer()
{
    return _bufferDisturb;
}
	
void Waves::init()
{
    // 创建波浪的根签名
    _disturbRS.Reset(2, 0);
    _disturbRS[0].InitAsConstants(0, 3);    // 3个32位的常量
    _disturbRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);       // 输出的数值
    _disturbRS.Finalize(L"disturb RS");

    // 创建波浪的PSO
    _disturbPSO.SetRootSignature(_disturbRS);
    _disturbPSO.SetComputeShader(g_pwaveDisturbCS, sizeof(g_pwaveDisturbCS));
    _disturbPSO.Finalize();

    // 创建更新波浪顶点的根签名
    _updateRS.Reset(4, 0);
    _updateRS[0].InitAsConstants(0, 3);
    _updateRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    _updateRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
    _updateRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 1);
    _updateRS.Finalize(L"update RS");

    // 创建更新波浪顶点的PSO
    _updatePSO.SetRootSignature(_updateRS);
    _updatePSO.SetComputeShader(g_pwaveUpdateCS, sizeof(g_pwaveUpdateCS));
    _updatePSO.Finalize();

    // 存储disturb的输出，然后作为update的输入
    _bufferDisturb.Create(L"bufferOut", mNumCols, mNumRows, 1, DXGI_FORMAT_R32_FLOAT);
    _bufferPre.Create(L"bufferPre", mNumCols, mNumRows, 1, DXGI_FORMAT_R32_FLOAT);
    _bufferWaves.Create(L"bufferWaves", mNumCols, mNumRows, 1, DXGI_FORMAT_R32_FLOAT);
}