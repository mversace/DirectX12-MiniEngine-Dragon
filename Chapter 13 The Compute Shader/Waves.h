//***************************************************************************************
// Waves.h by Frank Luna (C) 2011 All Rights Reserved.
//
// Performs the calculations for the wave simulation.  After the simulation has been
// updated, the client must copy the current solution into vertex buffers for rendering.
// This class only does the calculations, it does not do any drawing.
//***************************************************************************************

#ifndef WAVES_H
#define WAVES_H

#include <vector>
#include <DirectXMath.h>
#include "RootSignature.h"
#include "PipelineState.h"
#include "ColorBuffer.h"

class Waves
{
public:
    Waves(int m, int n, float dx, float dt, float speed, float damping);
    Waves(const Waves& rhs) = delete;
    Waves& operator=(const Waves& rhs) = delete;
    ~Waves();

	int RowCount()const;
	int ColumnCount()const;
	int VertexCount()const;
	int TriangleCount()const;
	float Width()const;
	float Depth()const;
    float SpatialStep()const;

	void Update(float dt);
	void Disturb(int i, int j, float magnitude);
    ColorBuffer& getWavesBuffer();

public:
    void init();
    void Destory();

private:
    int mNumRows = 0;
    int mNumCols = 0;

    int mVertexCount = 0;
    int mTriangleCount = 0;

    // Simulation constants we can precompute.
    float mK1 = 0.0f;
    float mK2 = 0.0f;
    float mK3 = 0.0f;

    float mTimeStep = 0.0f;
    float mSpatialStep = 0.0f;

    // 随机生成波浪的根签名和流水线
    RootSignature _disturbRS;
    ComputePSO _disturbPSO;

    // 更新波浪顶点的根签名和流水线
    RootSignature _updateRS;
    ComputePSO _updatePSO;

    ColorBuffer _bufferDisturb; // disturb的输出，同时也是update的输入
    ColorBuffer _bufferPre;     // 记录上一次的顶点信息
    ColorBuffer _bufferWaves;   // update生成的最新的顶点信息
};

#endif // WAVES_H