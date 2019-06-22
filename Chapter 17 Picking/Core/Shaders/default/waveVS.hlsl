cbuffer VSConstants : register(b0)
{
    float4x4 modelToWorld;
    float4x4 gTexTransform;
    float4x4 gMatTransform;
    float2 gDisplacementMapTexelSize;
    float gGridSpatialStep;
    float cbPerObjectPad1;
};

cbuffer PassConstants : register(b1)
{
    float4x4 gViewProj;
};

Texture2D    gDisplacementMap : register(t1);
SamplerState gsamLinearWrap : register(s1);
SamplerState gsamPointClamp : register(s2);

struct VertexIn
{
    float3 PosL  : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // Sample the displacement map using non-transformed [0,1]^2 tex-coords.
    vin.PosL.y += gDisplacementMap.SampleLevel(gsamLinearWrap, vin.TexC, 1.0f).r;

    // Estimate normal using finite difference.
    float du = gDisplacementMapTexelSize.x;
    float dv = gDisplacementMapTexelSize.y;
    float l = gDisplacementMap.SampleLevel(gsamPointClamp, vin.TexC - float2(du, 0.0f), 0.0f).r;
    float r = gDisplacementMap.SampleLevel(gsamPointClamp, vin.TexC + float2(du, 0.0f), 0.0f).r;
    float t = gDisplacementMap.SampleLevel(gsamPointClamp, vin.TexC - float2(0.0f, dv), 0.0f).r;
    float b = gDisplacementMap.SampleLevel(gsamPointClamp, vin.TexC + float2(0.0f, dv), 0.0f).r;
    vin.NormalL = normalize(float3(-r + l, 2.0f * gGridSpatialStep, b - t));

    // 把顶点转换到世界坐标系
    float4 posW = mul(float4(vin.PosL, 1.0f), modelToWorld);
    vout.PosW = posW.xyz;

    // 法向量转换到世界坐标系
    vout.NormalW = mul(vin.NormalL, (float3x3)modelToWorld);

    // 顶点转换到投影坐标系
    vout.PosH = mul(posW, gViewProj);

    // 直接返回纹理
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}