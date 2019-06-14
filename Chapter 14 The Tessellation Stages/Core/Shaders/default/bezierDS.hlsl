
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
#define NUM_CONTROL_POINTS 16

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(invT * invT * invT,
        3.0f * t * invT * invT,
        3.0f * t * t * invT,
        t * t * t);
}

float3 CubicBezierSum(const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum = basisV.x * (basisU.x * bezpatch[0].vPosition + basisU.y * bezpatch[1].vPosition + basisU.z * bezpatch[2].vPosition + basisU.w * bezpatch[3].vPosition);
    sum += basisV.y * (basisU.x * bezpatch[4].vPosition + basisU.y * bezpatch[5].vPosition + basisU.z * bezpatch[6].vPosition + basisU.w * bezpatch[7].vPosition);
    sum += basisV.z * (basisU.x * bezpatch[8].vPosition + basisU.y * bezpatch[9].vPosition + basisU.z * bezpatch[10].vPosition + basisU.w * bezpatch[11].vPosition);
    sum += basisV.w * (basisU.x * bezpatch[12].vPosition + basisU.y * bezpatch[13].vPosition + basisU.z * bezpatch[14].vPosition + basisU.w * bezpatch[15].vPosition);

    return sum;
}

float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(-3 * invT * invT,
        3 * invT * invT - 6 * t * invT,
        6 * t * invT - 3 * t * t,
        3 * t * t);
}

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,      // 曲面细分因子
	float2 domain : SV_DomainLocation,  // 新插入顶点的uv坐标(基于面片内部)
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)   // 原始面片的4个点
{
	DS_OUTPUT Output;

    float4 basisU = BernsteinBasis(domain.x);
    float4 basisV = BernsteinBasis(domain.y);

    float3 p = CubicBezierSum(patch, basisU, basisV);

    // 转世界坐标系
    float4 posW = mul(float4(p, 1.0f), gWorld);

    // 转观察坐标系
    Output.vPosition = mul(posW, gViewProj);

	return Output;
}
