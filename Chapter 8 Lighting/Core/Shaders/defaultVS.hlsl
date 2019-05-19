cbuffer VSConstants : register(b0)
{
    float4x4 modelToWorld;
};

cbuffer PassConstants : register(b1)
{
    float4x4 gViewProj;
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

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0), modelToWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    // Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;

    return vout;
}