// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

// Constant data that varies per frame.
cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float pad;
    float4 gAmbientLight;

    // Allow application to change fog parameters once per frame.
    // For example, we may only use fog for certain times of day.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 pad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 SizeW   : SIZE;
};

struct GeoOut
{
    float4 PosH    : SV_POSITION;       // 顶点的齐次坐标
    float3 PosW    : POSITION;          // 顶点的世界坐标
    float3 NormalW : NORMAL;            // 顶点的世界法向量
    float2 TexC    : TEXCOORD;          // 顶点的纹理坐标
    uint   PrimID  : SV_PrimitiveID;    // 顶点ID
};

[maxvertexcount(4)]
void main(
	point VertexOut gin[1],
    uint primID : SV_PrimitiveID,
	inout TriangleStream< GeoOut > triStream
)
{
    // 计算up向量
    float3 up = float3(0.0f, 1.0f, 0.0f);
    // 计算目标点到观察点的向量
    float3 look = gEyePosW - gin[0].CenterW;
    // 保证目标点和观察点在通一个xz平面
    look.y = 0.0f;
    // 标准化
    look = normalize(look);
    // 计算右向量
    float3 right = cross(up, look);

    // 计算公告板树的宽和高
    float halfWidth = 0.5f * gin[0].SizeW.x;
    float halfHeight = 0.5f * gin[0].SizeW.y;

    // 计算树的4个顶点
    float4 v[4];
    v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(gin[0].CenterW - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);

    // 四个点对应的纹理坐标
    float2 texC[4] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f)
    };

    // 输出图源
    GeoOut gout;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        gout.PosH = mul(v[i], gViewProj);   // 顶点由世界坐标系转到投影坐标系
        gout.PosW = v[i].xyz;               // 顶点的世界坐标
        gout.NormalW = look;                // 顶点的法向量
        gout.TexC = texC[i];                // 顶点的纹理
        gout.PrimID = primID;               // 该顶点所组成的面的ID

        triStream.Append(gout);
    }
}