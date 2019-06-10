#pragma once

#include "GameCore.h"
#include "ColorBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"

class SobelFilter
{
public:
    SobelFilter() = default;

public:
    void init(DXGI_FORMAT format);
    void destroy();

    void update(int w, int h);
    void doSobel(ColorBuffer& inBuff);

private:
    int _w = 0;
    int _h = 0;
    DXGI_FORMAT _format;

    ColorBuffer _buffer;

    RootSignature _rootSignature;
    ComputePSO _sobelPSO;
    ComputePSO _compositePSO;
};