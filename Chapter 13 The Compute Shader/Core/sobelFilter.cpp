#include "sobelFilter.h"
#include "CommandContext.h"

#include "CompiledShaders/sobelCS.h"
#include "CompiledShaders/compositeCS.h"

void SobelFilter::init(DXGI_FORMAT format)
{
    _format = format;

    // 创建根签名
    _rootSignature.Reset(2, 0);
    _rootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    _rootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    _rootSignature.Finalize(L"sobel RS");

    // 创建PSO
    _sobelPSO.SetRootSignature(_rootSignature);
    _sobelPSO.SetComputeShader(g_psobelCS, sizeof(g_psobelCS));
    _sobelPSO.Finalize();

    _compositePSO.SetRootSignature(_rootSignature);
    _compositePSO.SetComputeShader(g_pcompositeCS, sizeof(g_pcompositeCS));
    _compositePSO.Finalize();
}

void SobelFilter::destroy()
{
    _buffer.Destroy();
}

void SobelFilter::update(int w, int h)
{
    if (_w == h || _h == h)
        return;

    _w = w;
    _h = h;
    _buffer.Create(L"sobel buffer", _w, _h, 1, _format);
}

void SobelFilter::doSobel(ColorBuffer& inBuff)
{
    ComputeContext& context = ComputeContext::Begin(L"sobel");

    context.SetRootSignature(_rootSignature);

    // 传入的作为写入，把计算过的值输出到_buffer中
    context.SetPipelineState(_sobelPSO);
    context.TransitionResource(inBuff, D3D12_RESOURCE_STATE_GENERIC_READ);
    context.SetDynamicDescriptor(0, 0, inBuff.GetSRV());
    context.TransitionResource(_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.SetDynamicDescriptor(1, 0, _buffer.GetUAV());
    context.Dispatch((UINT)ceilf(_w / 16.0f), (UINT)ceilf(_h / 16.0f), 1);

    // 再把_buffer作为输入，inBuff作为输出，像素相乘
    context.SetPipelineState(_compositePSO);
    context.TransitionResource(_buffer, D3D12_RESOURCE_STATE_GENERIC_READ);
    context.SetDynamicDescriptor(0, 0, _buffer.GetSRV());
    context.TransitionResource(inBuff, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.SetDynamicDescriptor(1, 0, inBuff.GetUAV());
    context.Dispatch((UINT)ceilf(_w / 256.0f), _h, 1);

    context.Finish(true);
}