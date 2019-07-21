
struct VertexIn
{
    float3 PosL  : POSITION;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float2 TexC    : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // Already in homogeneous clip space.
    vout.PosH = float4(vin.PosL, 1.0f);

    vout.TexC = vin.TexC;

    return vout;
}