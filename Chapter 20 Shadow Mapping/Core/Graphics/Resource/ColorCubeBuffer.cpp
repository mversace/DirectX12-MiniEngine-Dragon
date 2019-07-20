#include "pch.h"
#include "ColorCubeBuffer.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "EsramAllocator.h"

using namespace Graphics;

void ColorCubeBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
{
    ID3D12Resource* Resource = m_pResource.Get();

    // Create the shader resource view
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = Format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.TextureCube.MipLevels = 1;

    if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_SRVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);

    // Create the render target view
    for (int i = 0; i < 6; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
        RTVDesc.Format = Format;
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice = 0;
        RTVDesc.Texture2DArray.PlaneSlice = 0;
        RTVDesc.Texture2DArray.FirstArraySlice = i;
        RTVDesc.Texture2DArray.ArraySize = 1;

        if (m_RTVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            m_RTVHandle[i] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle[i]);
    }
}

void ColorCubeBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
    DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMem)
{
    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 6, NumMips, Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    ResourceDesc.SampleDesc.Count = 1;
    ResourceDesc.SampleDesc.Quality = 0;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = Format;
    ClearValue.Color[0] = m_ClearColor.R();
    ClearValue.Color[1] = m_ClearColor.G();
    ClearValue.Color[2] = m_ClearColor.B();
    ClearValue.Color[3] = m_ClearColor.A();

    CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMem);
    CreateDerivedViews(Graphics::g_Device, Format, 6, NumMips);
}