#pragma once

#include <string>
#include <unordered_map>
#include "VectorMath.h"

struct ObjectConstants
{
    Math::Matrix4 World = Math::Matrix4(Math::kIdentity); // 把物体从模型坐标转换到世界坐标
};

struct PassConstants
{
    Math::Matrix4 viewProj = Math::Matrix4(Math::kIdentity);    // 从世界坐标转为投影坐标的矩阵
    Math::Vector3 eyePosW = { 0.0f, 0.0f, 0.0f };               // 观察点也就是摄像机位置
};

// 以下为绘制使用

// 顶点结构
struct Vertex
{
    Vertex() = default;
    Vertex(float x, float y, float z) :
        Pos(x, y, z) {}

    DirectX::XMFLOAT3 Pos;
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

private:
    StructuredBuffer vertexBuff;    // 顶点buff
    ByteAddressBuffer indexBuff;    // 索引buff
};

struct RenderItem
{
    Math::Matrix4 modeToWorld = Math::Matrix4(Math::kIdentity);      // 模型坐标转世界坐标矩阵

    int IndexCount = 0;             // 索引个数
    int StartIndexLocation = 0;     // 索引起始位置
    int BaseVertexLocation = 0;     // 顶点起始位置
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    MeshGeometry* geo = nullptr;    // 几何结构指针，包含对应的顶点以及索引
};