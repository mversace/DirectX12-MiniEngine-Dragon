struct VertexIn
{
    float3 PosW  : POSITION;    // 顶点的世界坐标
    float2 SizeW : SIZE;        // 顶点的宽高
};

struct VertexOut
{
    float3 CenterW : POSITION;  // 中心点的世界坐标
    float2 SizeW   : SIZE;      // 宽高
};

VertexOut main(VertexIn vin)
{
    VertexOut vout;

    // 顶点直接传给几何着色器
    vout.CenterW = vin.PosW;
    vout.SizeW = vin.SizeW;

    return vout;
}