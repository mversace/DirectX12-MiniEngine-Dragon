//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"
#include "common.hlsl"

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
Texture2D gTextureMaps[6] : register(t1);
// gTextureMaps占据的是t1-t7，而天空盒纹理本身是放在t4位置，这里转成cube类型
TextureCube gCubeMap : register(t7);
// 阴影纹理
Texture2D gShadowMap : register(t8);

SamplerState gsamLinearWrap  : register(s0);
SamplerState gsamAnisotropicWrap  : register(s1);
SamplerComparisonState gsamShadow  : register(s2);

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }

    return percentLit / 9.0f;
}

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

// Constant data that varies per frame.
cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float4x4 gModelToShadow;
    float3 gEyePosW;
    float pad;
    float4 gAmbientLight;

    // Allow application to change fog parameters once per frame.
    // For example, we may only use fog for certain times of day.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 pad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float4 ShadowPosH : POSITION0;
    float3 PosW    : POSITION1;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT; // 切线的世界向量
    float2 TexC    : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target0
{
    // 获取该纹理的参数
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float  roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;

    // 法向量规范化
    pin.NormalW = normalize(pin.NormalW);

    // 取得法向纹理
    float4 normalMapSample = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    // 计算出该点的实际法向 取代pin.NormalW
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

    //bumpedNormalW = pin.NormalW;

    diffuseAlbedo *= gTextureMaps[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);

    // 透明像素点剔除
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);

    // Vector from point being lit to eye. 
    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye; // normalize

    // Indirect lighting.
    float4 ambient = gAmbientLight * diffuseAlbedo;

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

    const float shininess = (1.0f - roughness) * normalMapSample.a;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // 雾，如果雾的颜色alpha为0，则不处理
    if (gFogColor.a > 0.01)
    {
        float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
        litColor = lerp(litColor, gFogColor, fogAmount);
    }

    // 镜面反射
    float3 r = reflect(-toEyeW, bumpedNormalW);
    float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
    
    // Common convention to take alpha from diffuse material.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}