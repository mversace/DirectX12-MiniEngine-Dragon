
cbuffer cbUpdateSettings : register(b0)
{
    float gDisturbMag;      // 浪的高度
    int2 gDisturbIndex;     // 顶点xy坐标
};

// 存储顶点高度y的一维数组
RWTexture2D<float> gOutput : register(u0);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    int x = gDisturbIndex.x;
    int y = gDisturbIndex.y;

    float halfMag = 0.5f * gDisturbMag;

    // 自身顶点提高gDisturbMag
    gOutput[int2(x, y)] += gDisturbMag;
    // 周围顶点提高gDisturbMag/2
    gOutput[int2(x + 1, y)] += halfMag;
    gOutput[int2(x - 1, y)] += halfMag;
    gOutput[int2(x, y + 1)] += halfMag;
    gOutput[int2(x, y - 1)] += halfMag;
}