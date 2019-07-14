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
    float3 gEyePosW;
    float pad;
    // ...
};

struct VertexIn
{
    float3 PosL  : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosL    : POSITION;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // 使用模型坐标系
    vout.PosL = vin.PosL;

    // 把顶点转换到世界坐标系
    float4 posW = mul(float4(vin.PosL, 1.0f), modelToWorld);
    
    // 保持天空盒的中心始终在摄像机的位置
    posW.xyz += gEyePosW;

    // 顶点转换到投影坐标系
    // 使得z=w，这样始终在最远的平面
    vout.PosH = mul(posW, gViewProj).xyww;

    return vout;
}