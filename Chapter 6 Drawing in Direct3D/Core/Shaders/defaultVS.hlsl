cbuffer VSConstants : register(b0)
{
    float4x4 modelToProjection;
};

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
    vout.PosH = mul(modelToProjection, float4(vin.PosL, 1.0));

    // Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;

    return vout;
}