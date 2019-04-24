//cbuffer cbPerObject : register(b0)
//{
//    float4x4 gWorldViewProj;
//};

struct VertexIn
{
    float3 PosL  : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout;

    // Transform to homogeneous clip space.
//    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);

    vout.PosH = float4(vin.PosL, 1.0f);
    // Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;

    return vout;
}