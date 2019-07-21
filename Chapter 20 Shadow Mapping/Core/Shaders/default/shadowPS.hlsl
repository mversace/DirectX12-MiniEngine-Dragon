//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

struct MaterialData
{
    float4   DiffuseAlbedo;
    float3   FresnelR0;
    float    Mpad0;   // 占位符
    float    Roughness;
    uint     DiffuseMapIndex;   // 纹理ID
    uint     NormalMapIndex;    // 法向贴图的ID
    float    Mpad2;   // 占位符
};

StructuredBuffer<MaterialData> gMaterialData : register(t0);

// 占据t1-t7
Texture2D gTextureMaps[7] : register(t1);

SamplerState gsamAnisotropicWrap  : register(s1);

cbuffer VSConstants : register(b0)
{
    float4x4 modelToWorld;
    float4x4 gTexTransform;
    float4x4 gMatTransform;
    uint gMaterialIndex;
    uint vPad0;   // 占位符
    uint vPad1;   // 占位符
    uint vPad2;   // 占位符
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float2 TexC    : TEXCOORD;
};

void main(VertexOut pin)
{
    // 获取该纹理的参数
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseTexIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= gTextureMaps[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);

    // 这里仅仅是为了剔除一些像素
    // 透明像素点剔除
    //clip(diffuseAlbedo.a - 0.1f);
}