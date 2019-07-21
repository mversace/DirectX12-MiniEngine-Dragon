cbuffer VSConstants : register(b0)
{
    float4x4 modelToWorld;
    float4x4 gTexTransform;
    float4x4 gMatTransform;
    uint gMaterialIndex;
};

cbuffer PassConstants : register(b1)
{
    float4x4 gViewProj;
};

struct VertexIn
{
    float3 PosL  : POSITION;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float2 TexC    : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // 把顶点转换到世界坐标系
    float4 posW = mul(float4(vin.PosL, 1.0f), modelToWorld);

    // 顶点转换到投影坐标系
    vout.PosH = mul(posW, gViewProj);

    // 直接返回纹理
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}