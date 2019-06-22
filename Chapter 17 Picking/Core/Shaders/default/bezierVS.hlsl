struct VertexIn
{
    float3 PosL    : POSITION;
};

struct VertexOut
{
    float3 PosL    : POSITION;
};

VertexOut main(VertexIn vin)
{
    // do nothing
    VertexOut vout;

    vout.PosL = vin.PosL;

    return vout;
}