
// Texture2D gDiffuseMap[7] : register(t1);
// gDiffuseMap占据的是t1-t7，而天空盒纹理本身是放在t7位置，这里转成cube类型
TextureCube gCubeMap : register(t7);

SamplerState gsamLinearWrap  : register(s0);

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosL    : POSITION;
};

float4 main(VertexOut pin) : SV_Target0
{
    return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}