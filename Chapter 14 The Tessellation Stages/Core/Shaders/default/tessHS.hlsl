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

// 输入控制点
struct VS_CONTROL_POINT_OUTPUT
{
	float3 vPosition : POSITION;    // 面片的顶点(模型坐标系顶点)
};

// 输出控制点
struct HS_CONTROL_POINT_OUTPUT
{
	float3 vPosition : POSITION;    // 输出的顶点(模型坐标系顶点)
};

// 输出修补程序常量数据。
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor; // 例如，对于四象限域，将为 [4]
	float InsideTessFactor[2]		: SV_InsideTessFactor; // 例如，对于四象限域，将为 Inside[2]
	// TODO:  更改/添加其他资料
};

// 4个控制点的面片
#define NUM_CONTROL_POINTS 4

// 修补程序常量函数
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

    // 计算面片中心点
    float3 centerL = 0.25f * (ip[0].vPosition + ip[1].vPosition + ip[2].vPosition + ip[3].vPosition);
    // 中心点转成世界坐标系的点
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;

    // 计算与摄像机的距离
    float d = distance(centerW, gEyePosW);

    // 根据距离调整细分的三角形数量
    const float d0 = 20.0f;
    const float d1 = 100.0f;
    // saturate结果限定在[0.0, 1.0]中
    float tess = 64.0f * saturate((d1 - d) / (d1 - d0));

	// 在此处插入代码以计算输出
	Output.EdgeTessFactor[0] = 
		Output.EdgeTessFactor[1] = 
		Output.EdgeTessFactor[2] = 
        Output.EdgeTessFactor[3] = 
        Output.InsideTessFactor[0] = 
		Output.InsideTessFactor[1] = tess; // 例如，可改为计算动态分割因子

	return Output;
}

[domain("quad")]                                // 四边形面片
[partitioning("integer")]                       // 细分模式 fractional_even fractional_odd
[outputtopology("triangle_cw")]                 // 通过细分创建的三角形顶点序
[outputcontrolpoints(4)]                        // 输出4个顶点
[patchconstantfunc("CalcHSPatchConstants")]     // 常量外壳着色器函数名
HS_CONTROL_POINT_OUTPUT main( 
	InputPatch<VS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	HS_CONTROL_POINT_OUTPUT Output;

	// 在此处插入代码以计算输出
	Output.vPosition = ip[i].vPosition;

	return Output;
}
