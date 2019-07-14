
// Texture2D gDiffuseMap[4] : register(t1);
// gDiffuseMap占据的是t1-t4，而天空盒纹理本身是放在t4位置，这里转成cube类型
TextureCube gCubeMap : register(t4);

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