//=============================================================================
// Performs a separable Guassian blur with a blur radius up to 5 pixels.
//=============================================================================

cbuffer cbSettings : register(b0)
{
    // 模糊半径
    int gBlurRadius;

    // 最多支持一次11个像素权重
    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};

// 最多支持的模糊半径=5
static const int gMaxBlurRadius = 5;

Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 为了保证后续处理的统一，也就是每次都能计算gBlurRadius*2+1个权重
// gCache的长度需要位该线程组所处理像素+gBlurRadius*2
// 这样边缘的模糊也能统一处理
#define N 256
#define CacheSize (N + 2*gMaxBlurRadius)
groupshared float4 gCache[CacheSize];

[numthreads(N, 1, 1)]
void main(int3 groupThreadID : SV_GroupThreadID,
    int3 dispatchThreadID : SV_DispatchThreadID)
{
    // 记录权重值
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

    if (groupThreadID.x < gBlurRadius)
    {
        // 左边如果出界，就采用边缘的值填充
        int x = max(dispatchThreadID.x - gBlurRadius, 0);
        gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
    }
    if (groupThreadID.x >= N - gBlurRadius)
    {
        // 右边如果出界，就采用边缘的值填充
        int x = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x - 1);
        gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
    }

    // 每个线程组固定开启N个线程，处理N个像素，有可能像素不够，这里也需要处理下
    gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];

    // 等待所有线程结束，也就是gCache填好了对应的像素
    GroupMemoryBarrierWithGroupSync();

    // 根据传入的权重，输出最终值
    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        int k = groupThreadID.x + gBlurRadius + i;

        blurColor += weights[i + gBlurRadius] * gCache[k];
    }

    gOutput[dispatchThreadID.xy] = blurColor;
}