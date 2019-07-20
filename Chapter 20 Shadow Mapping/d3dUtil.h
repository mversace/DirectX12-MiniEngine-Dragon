#pragma once

#include <string>
#include <unordered_map>
#include "VectorMath.h"

// 雾的浓度
static float flFrogAlpha = 0.0f;
// 是否开启视锥体剔除
static bool g_openFrustumCull = true;

// 与HLSL一致
struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.0f, 0.0f, 0.05f };
    float FalloffStart = 0.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, 0.0f, 0.0f };          // directional/spot light only
    float FalloffEnd = 0.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };            // point/spot light only
    float SpotPower = 0;                            // spot light only
};

#define MaxLights 16

struct ObjectConstants
{
    Math::Matrix4 World = Math::Matrix4(Math::kIdentity); // 把物体从模型坐标转换到世界坐标
    Math::Matrix4 texTransform = Math::Matrix4(Math::kIdentity); // 该顶点所用纹理的转换矩阵
    Math::Matrix4 matTransform = Math::Matrix4(Math::kIdentity);
    UINT MaterialIndex;
};

struct PassConstants
{
    Math::Matrix4 viewProj = Math::Matrix4(Math::kIdentity);      // 从世界坐标转为投影坐标的矩阵
    Math::Vector3 eyePosW = { 0.0f, 0.0f, 0.0f };     // 观察点也就是摄像机位置
    Math::Vector4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    Math::Vector4 FogColor = { 0.7f, 0.7f, 0.7f, flFrogAlpha };
    float gFogStart = 50.0f;
    float gFogRange = 200.0f;
    DirectX::XMFLOAT2 pad;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

struct MaterialConstants
{
    Math::Vector4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };  // 占据16字节
    Math::Vector3 FresnelR0 = { 0.01f, 0.01f, 0.01f };  // 占据16字节
    float Roughness = 0.25f;
    UINT DiffuseMapIndex = 0;   // 该纹理数据对应的纹理贴图id
    UINT NormalSrvHeapIndex = 0; // 该纹理数据对应的纹理法相贴图id
    UINT MaterialPad1;
};


// 以下为绘制使用

// 顶点结构
struct Vertex
{
    Vertex() = default;

    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
    DirectX::XMFLOAT3 TangentU;
};

// 每一个子目标的结构体
struct SubmeshGeometry
{
    int IndexCount = 0;
    int StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class StructuredBuffer;
class ByteAddressBuffer;
// 绘制目标的几何结构
class MeshGeometry
{
public:
    MeshGeometry() = default;
    virtual ~MeshGeometry()
    {
        
    }

public:
    void createVertex(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize,
        const void* initialData = nullptr)
    {
        vertexBuff.Create(name, NumElements, ElementSize, initialData);
        vertexView = vertexBuff.VertexBufferView();
    }

    void createIndex(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize,
        const void* initialData = nullptr)
    {
        indexBuff.Create(name, NumElements, ElementSize, initialData);
        indexView = indexBuff.IndexBufferView();
    }

    void storeVertexAndIndex(std::vector<Vertex>& vertex, std::vector<std::int32_t>& index)
    {
        vecVertex = std::move(vertex);
        vecIndex = std::move(index);
    }

    void destroy()
    {
        vertexBuff.Destroy();
        indexBuff.Destroy();
    }

public:
    std::string name;

    std::unordered_map<std::string, SubmeshGeometry> geoMap;    // 使用该顶点和索引的物体

    D3D12_VERTEX_BUFFER_VIEW vertexView;
    D3D12_INDEX_BUFFER_VIEW indexView;

    // 存储顶点和索引数据
    std::vector<Vertex> vecVertex;
    std::vector<std::int32_t> vecIndex;

private:
    StructuredBuffer vertexBuff;    // 顶点buff
    ByteAddressBuffer indexBuff;    // 索引buff
};

struct RenderItem
{
    Math::Matrix4 modeToWorld = Math::Matrix4(Math::kIdentity);      // 模型坐标转世界坐标矩阵
    Math::Matrix4 texTransform = Math::Matrix4(Math::kIdentity);     // 纹理转换矩阵，主要用于顶点对应纹理的缩放
    Math::Matrix4 matTransform = Math::Matrix4(Math::kIdentity);     // 纹理额外控制矩阵，比如通过这个矩阵来动态移动纹理

    UINT MaterialIndex = -1;           // 索引，本项目是索引对应的纹理数据

    int IndexCount = 0;             // 索引个数
    int StartIndexLocation = 0;     // 索引起始位置
    int BaseVertexLocation = 0;     // 顶点起始位置
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    MeshGeometry* geo = nullptr;    // 几何结构指针，包含对应的顶点以及索引
};