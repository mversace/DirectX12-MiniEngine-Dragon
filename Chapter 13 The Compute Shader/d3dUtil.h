#pragma once

#include <string>
#include <unordered_map>
#include "VectorMath.h"

// 模糊次数
static int g_blurCount = 0;

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

__declspec(align(16)) struct ObjectConstants
{
    Math::Matrix4 World = Math::Matrix4(Math::kIdentity); // 把物体从模型坐标转换到世界坐标
    Math::Matrix4 texTransform = Math::Matrix4(Math::kIdentity); // 该顶点所用纹理的转换矩阵
    Math::Matrix4 matTransform = Math::Matrix4(Math::kIdentity);
    DirectX::XMFLOAT2 DisplacementMapTexelSize = { 1.0f, 1.0f };
    float GridSpatialStep = 1.0f;
    float Pad;
};

__declspec(align(16)) struct PassConstants
{
    Math::Matrix4 viewProj = Math::Matrix4(Math::kIdentity);      // 从世界坐标转为投影坐标的矩阵
    Math::Vector3 eyePosW = { 0.0f, 0.0f, 0.0f };     // 观察点也就是摄像机位置
    Math::Vector4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    Math::Vector4 FogColor = { 0.7f, 0.7f, 0.7f, 0.0f };
    float gFogStart = 50.0f;
    float gFogRange = 200.0f;
    DirectX::XMFLOAT2 pad;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

__declspec(align(16)) struct MaterialConstants
{
    Math::Vector4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    Math::Vector3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;
};


// 以下为绘制使用

// 顶点结构
struct Vertex
{
    Vertex() = default;
    Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
        Pos(x, y, z),
        Normal(nx, ny, nz),
        TexC(u, v) {}

    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
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

    bool bDynamicVertex = false;    // 是否动态顶点
    std::vector<Vertex> vecVertex;

private:
    StructuredBuffer vertexBuff;    // 顶点buff
    ByteAddressBuffer indexBuff;    // 索引buff
};

struct Material
{
    std::string name;

    Math::Vector4 diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };   // 漫反射系数
    Math::Vector3 fresnelR0 = { 0.01f, 0.01f, 0.01f };  // 反射系数
    float roughness = 0.25f;    // 粗糙度

    D3D12_CPU_DESCRIPTOR_HANDLE srv;    // 纹理视图
};

struct RenderItem
{
    Math::Matrix4 modeToWorld = Math::Matrix4(Math::kIdentity);      // 模型坐标转世界坐标矩阵
    Math::Matrix4 texTransform = Math::Matrix4(Math::kIdentity);     // 纹理转换矩阵，主要用于顶点对应纹理的缩放
    Math::Matrix4 matTransform = Math::Matrix4(Math::kIdentity);     // 纹理额外控制矩阵，比如通过这个矩阵来动态移动纹理

    DirectX::XMFLOAT2 DisplacementMapTexelSize = { 1.0f, 1.0f };
    float GridSpatialStep = 1.0f;

    int IndexCount = 0;             // 索引个数
    int StartIndexLocation = 0;     // 索引起始位置
    int BaseVertexLocation = 0;     // 顶点起始位置
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    MeshGeometry* geo = nullptr;    // 几何结构指针，包含对应的顶点以及索引
    Material* mat = nullptr;        // 纹理指针，包含该渲染目标的纹理属性以及纹理视图
};