
Texture2D gShadowMap : register(t8);

SamplerState gsamLinearWrap  : register(s0);

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float2 TexC    : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target0
{
    return float4(gShadowMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
}