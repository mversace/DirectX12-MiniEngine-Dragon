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

	// 在此处插入代码以计算输出
	Output.EdgeTessFactor[0] = 
		Output.EdgeTessFactor[1] = 
		Output.EdgeTessFactor[2] = 
        Output.EdgeTessFactor[3] = 
        Output.InsideTessFactor[0] = 
		Output.InsideTessFactor[1] = 25; // 例如，可改为计算动态分割因子

	return Output;
}

[domain("quad")]                                // 四边形面片
[partitioning("integer")]                       // 细分模式 fractional_even fractional_odd
[outputtopology("triangle_cw")]                 // 通过细分创建的三角形顶点序
[outputcontrolpoints(16)]                       // 输出16个顶点
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
