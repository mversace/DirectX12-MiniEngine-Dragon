
// 每个顶点的结构化属性
struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    float4x4 MatTransform;
    uint     MaterialIndex;
    uint     InstPad0;
    uint     InstPad1;
    uint     InstPad2;
};

// 所有顶点的结构化数据
StructuredBuffer<InstanceData> gInstanceData : register(t0);

// 常量缓冲区
cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
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
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;

    // 不允许篡改，插值
    nointerpolation uint MatIndex  : MATINDEX;
};

VertexOut main(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut)0.0f;

    // 取出该顶点的数据
    InstanceData instData = gInstanceData[instanceID];
    float4x4 modelToWorld = instData.World;
    float4x4 texTransform = instData.TexTransform;
    float4x4 matTransform = instData.MatTransform;
    vout.MatIndex = instData.MaterialIndex;

    // 把顶点转换到世界坐标系
    float4 posW = mul(float4(vin.PosL, 1.0f), modelToWorld);
    vout.PosW = posW.xyz;

    // 法向量转换到世界坐标系
    vout.NormalW = mul(vin.NormalL, (float3x3)modelToWorld);

    // 顶点转换到投影坐标系
    vout.PosH = mul(posW, gViewProj);

    // 直接返回纹理
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform);
    vout.TexC = mul(texC, matTransform).xy;

    return vout;
}