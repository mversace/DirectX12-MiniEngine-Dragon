/*
	天空盒颜色缓冲区
	对像素缓冲区进一步封装，规格每个像素中存储的是颜色值

	维护视图：
	m_SRVHandle: 着色器资源视图
	m_RTVHandle[6]: 渲染目标视图

	CreateFromSwapChain： 封装交换链的缓冲区(常用)
	Create: 直接创建缓冲区
*/

#pragma once

#include "PixelBuffer.h"
#include "Color.h"

class EsramAllocator;

class ColorCubeBuffer : public PixelBuffer
{
public:
    ColorCubeBuffer( Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f)  )
        : m_ClearColor(ClearColor)
    {
        m_SRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        std::memset(m_RTVHandle, 0xFF, sizeof(m_RTVHandle));
    }

    // Create a color buffer.  If an address is supplied, memory will not be allocated.
    // The vmem address allows you to alias buffers (which can be especially useful for
    // reusing ESRAM across a frame.)
    void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
        DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);
 
    // Get pre-created CPU-visible descriptor handles
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(int idx = 0) const {
        if (idx < 0 || idx > 5)
            idx = 0;

        return m_RTVHandle[idx]; 
    }

    Color GetClearColor(void) const { return m_ClearColor; }

protected:
    void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);

    Color m_ClearColor;
    D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle[6];
};
