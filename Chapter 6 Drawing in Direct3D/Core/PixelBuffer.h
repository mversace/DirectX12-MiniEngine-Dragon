//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

/*
	像素缓冲区

	其实资源就是一块内存地址，也可以成为是buff，缓冲区等\

	这里就是再封装一层，主要处理的是像素缓冲区类型的资源

	这里主要实现了根据一些格式直接生成资源，或者把某个资源当作像素缓冲区管理起来
*/

#pragma once

#include "GpuResource.h"

class EsramAllocator;

class PixelBuffer : public GpuResource
{
public:
    PixelBuffer() : m_Width(0), m_Height(0), m_ArraySize(0), m_Format(DXGI_FORMAT_UNKNOWN), m_BankRotation(0) {}

    uint32_t GetWidth(void) const { return m_Width; }
    uint32_t GetHeight(void) const { return m_Height; }
    uint32_t GetDepth(void) const { return m_ArraySize; }
    const DXGI_FORMAT& GetFormat(void) const { return m_Format; }

    // Has no effect on Windows
    void SetBankRotation( uint32_t RotationAmount ) { m_BankRotation = RotationAmount; }

    // Write the raw pixel buffer contents to a file
    // Note that data is preceded by a 16-byte header:  { DXGI_FORMAT, Pitch (in pixels), Width (in pixels), Height }
    void ExportToFile( const std::wstring& FilePath );

protected:

	// 生成描述2d纹理的结构
    D3D12_RESOURCE_DESC DescribeTex2D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT Format, UINT Flags);

	// 把现成的Resource管理起来
    void AssociateWithResource( ID3D12Device* Device, const std::wstring& Name, ID3D12Resource* Resource, D3D12_RESOURCE_STATES CurrentState );

	// 生成一个纹理资源
    void CreateTextureResource( ID3D12Device* Device, const std::wstring& Name, const D3D12_RESOURCE_DESC& ResourceDesc,
        D3D12_CLEAR_VALUE ClearValue, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN );
	// 生成一个纹理资源
    void CreateTextureResource( ID3D12Device* Device, const std::wstring& Name, const D3D12_RESOURCE_DESC& ResourceDesc,
        D3D12_CLEAR_VALUE ClearValue, EsramAllocator& Allocator );

	// 以下主要是对当前像素缓冲区，像素格式的一些转换，方便一些操作
    static DXGI_FORMAT GetBaseFormat( DXGI_FORMAT Format );
    static DXGI_FORMAT GetUAVFormat( DXGI_FORMAT Format );
    static DXGI_FORMAT GetDSVFormat( DXGI_FORMAT Format );
    static DXGI_FORMAT GetDepthFormat( DXGI_FORMAT Format );
    static DXGI_FORMAT GetStencilFormat( DXGI_FORMAT Format );
	// 获得每个像素有多少个字节
    static size_t BytesPerPixel( DXGI_FORMAT Format );

    uint32_t m_Width;
    uint32_t m_Height;
    uint32_t m_ArraySize;
    DXGI_FORMAT m_Format;
    uint32_t m_BankRotation;
};
