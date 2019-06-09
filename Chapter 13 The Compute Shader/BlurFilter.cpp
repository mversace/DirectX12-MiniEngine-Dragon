#include "BlurFilter.h"
#include "CommandContext.h"

#include "CompiledShaders/blurHorzCS.h"
#include "CompiledShaders/blurVertCS.h"

void BlurFilter::init(DXGI_FORMAT format)
{
    _format = format;

    // 创建根签名
    _rootSignature.Reset(3, 0);
    _rootSignature[0].InitAsConstants(0, 12);
    _rootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    _rootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    _rootSignature.Finalize(L"blurRS");

    // 创建PSO
    _horzPSO.SetRootSignature(_rootSignature);
    _horzPSO.SetComputeShader(g_pblurHorzCS, sizeof(g_pblurHorzCS));
    _horzPSO.Finalize();

    _vertPSO.SetRootSignature(_rootSignature);
    _vertPSO.SetComputeShader(g_pblurVertCS, sizeof(g_pblurVertCS));
    _vertPSO.Finalize();
}

void BlurFilter::destory()
{
    _bufferA.Destroy();
    _bufferB.Destroy();
}

void BlurFilter::update(int w, int h)
{
    if (_w == w && _h == h)
        return;

    _w = w;
    _h = h;
    _bufferA.Create(L"blur A", _w, _h, 1, _format);
    _bufferB.Create(L"blur B", _w, _h, 1, _format);
}

void BlurFilter::doBlur(GpuResource& inBuff, int blurCount)
{
    auto weights = CalcGaussWeights(2.5f);
    int blurRadius = (int)weights.size() / 2;

    ComputeContext& context = ComputeContext::Begin(L"blur filter");

    // 把inBuff拷贝到bufferA中
    context.CopyBuffer(_bufferA, inBuff);

    context.SetRootSignature(_rootSignature);
    // 设置水平、垂直模糊公有的常量
    context.SetConstant(0, blurRadius);
    context.SetConstantArray(0, (UINT)weights.size(), weights.data(), 1);

    for (int i = 0; i < blurCount; ++i)
    {
        // 水平方向模糊
        context.SetPipelineState(_horzPSO);

        // 设置描述符常量
        context.TransitionResource(_bufferA, D3D12_RESOURCE_STATE_GENERIC_READ);
        context.SetDynamicDescriptor(1, 0, _bufferA.GetSRV());
        context.TransitionResource(_bufferB, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(2, 0, _bufferB.GetUAV());

        UINT numGroupsX = (UINT)ceilf(_w / 256.0f);
        context.Dispatch(numGroupsX, _h, 1);

        // 垂直方向模糊
        context.SetPipelineState(_vertPSO);

        // 设置描述符常量，这里AB反过来，最终_bufferA中是输出
        context.TransitionResource(_bufferB, D3D12_RESOURCE_STATE_GENERIC_READ);
        context.SetDynamicDescriptor(1, 0, _bufferB.GetSRV());
        context.TransitionResource(_bufferA, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.SetDynamicDescriptor(2, 0, _bufferA.GetUAV());

        UINT numGroupsY = (UINT)ceilf(_h / 256.0f);
        context.Dispatch(_w, numGroupsY, 1);
    }

    context.Finish(true);
}

std::vector<float> BlurFilter::CalcGaussWeights(float sigma)
{
    float twoSigma2 = 2.0f * sigma * sigma;

    // Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
    // For example, for sigma = 3, the width of the bell curve is 
    int blurRadius = (int)ceil(2.0f * sigma);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        float x = (float)i;

        weights[i + blurRadius] = expf(-x * x / twoSigma2);

        weightSum += weights[i + blurRadius];
    }

    // Divide by the sum so all the weights add up to 1.0.
    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= weightSum;
    }

    return weights;
}