
// 常量缓冲区b0
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

// 常量缓冲区b1
cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
};

struct DS_OUTPUT
{
	float4 vPosition  : SV_POSITION;
};

// 输出控制点
struct HS_CONTROL_POINT_OUTPUT
{
	float3 vPosition : POSITION;
};

// 输出修补程序常量数据。
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor; // 例如，对于四象限域，将为 [4]
	float InsideTessFactor[2]		: SV_InsideTessFactor; // 例如，对于四象限域，将为 Inside[2]
};

// 4个控制点的面片
#define NUM_CONTROL_POINTS 4

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,      // 曲面细分因子
	float2 domain : SV_DomainLocation,  // 新插入顶点的uv坐标(基于面片内部)
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)   // 原始面片的4个点
{
	DS_OUTPUT Output;

    // 双线性插值 lerp(x, y, s) = x + s(y - x)
    float3 v1 = lerp(patch[0].vPosition, patch[1].vPosition, domain.x);
    float3 v2 = lerp(patch[2].vPosition, patch[3].vPosition, domain.x);
    // 计算该顶点的实际位置(模型坐标系)
    float3 p = lerp(v1, v2, domain.y);

    // 适当修改y，以便高低起伏
    p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));

    // 转世界坐标系
    float4 posW = mul(float4(p, 1.0f), gWorld);

    // 转观察坐标系
    Output.vPosition = mul(posW, gViewProj);

	return Output;
}
