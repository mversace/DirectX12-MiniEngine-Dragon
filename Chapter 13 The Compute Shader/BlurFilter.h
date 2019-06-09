#pragma once

#include "GameCore.h"
#include "ColorBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuResource.h"

class BlurFilter
{
public:
    BlurFilter() = default;

public:
    void init(DXGI_FORMAT format);
    void destory();

    void update(int w, int h);
    void doBlur(GpuResource& inBuff, int blurCount);

    ColorBuffer& getOutBuffer()
    {
        return _bufferA;
    }

private:
    std::vector<float> CalcGaussWeights(float sigma);

private:
    int _w = 0;
    int _h = 0;
    DXGI_FORMAT _format;

    ColorBuffer _bufferA;
    ColorBuffer _bufferB;

    
    RootSignature _rootSignature;

    // 水平模糊
    ComputePSO _horzPSO;

    // 垂直模糊
    ComputePSO _vertPSO;
};