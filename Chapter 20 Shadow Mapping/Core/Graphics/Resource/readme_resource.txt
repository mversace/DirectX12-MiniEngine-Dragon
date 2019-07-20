
接口说明：
--资源：ID3D12Resource
--描述符句柄：D3D12_CPU_DESCRIPTOR_HANDLE

文件说明：
--EsramAllocator
--无用类，也许是微软暂时还没有实现

--GpuResource
--对ID3D12Resource的简单封装

/**********************************************************************
    2D纹理 D3D12_RESOURCE_DIMENSION_TEXTURE2D  DescribeTex2D()
**********************************************************************/
--PixelBuffer -> GpuResource
--像素缓冲区
--对于资源来说，很多就是gpu中的一块内存，可以叫buff、缓冲区等
--这里实现的就是像素缓冲区，规定该buff个结构是像素类型，规定了每个像素的格式

--ColorBuffer -> PixelBuffer -> GpuResource
--颜色缓冲区
--进一步规定了每个像素的结构是颜色格式
--还维护有3种描述符句柄：
----m_SRVHandle: 着色器资源视图句柄
----m_RTVHandle: 渲染目标视图	句柄		！！通过Create创建的缓冲区才会创建该视图
----m_UAVHandle[12]: 无序访问视图句柄		！！通过Create创建的缓冲区才会创建该视图

--ColorCubeBuffer -> PixelBuffer -> GpuResource
--天空盒颜色缓冲区
--进一步规定了每个像素的结构是颜色格式
--还维护有2种描述符句柄：
----m_SRVHandle: 着色器资源视图句柄
----m_RTVHandle[6]: 渲染目标视图句柄	

--DepthBuffer -> PixelBuffer -> GpuResource
--深度/模板缓冲区
--维护有3种描述符句柄：
----m_hDSV[4]: 4种不同意义的深度视图句柄
----m_hDepthSRV: 深度着色器资源视图句柄
----m_hStencilSRV: 模板着色器资源视图句柄

--ShadowBuffer -> DepthBuffer -> PixelBuffer -> GpuResource
--阴影 深度/模板缓冲区
--维护有3种描述符句柄：
----m_hDSV[4]: 4种不同意义的深度视图句柄
----m_hDepthSRV: 深度着色器资源视图句柄
----m_hStencilSRV: 模板着色器资源视图句柄


/**********************************************************************
    缓冲区 D3D12_RESOURCE_DIMENSION_BUFFER
**********************************************************************/
--GpuBuffer
--比较基础的缓冲区类

--ReadbackBuffer
--回写缓冲区类

--LinearAllocator
--buff布局：D3D12_TEXTURE_LAYOUT_ROW_MAJOR
--可以创建GPU可读的默认缓冲区、CPU\GPU可读的上传缓冲区
--整体实现类似CommandAllocator
--对外使用类：LinearAllocator
----典型的使用就是创建一个上传缓冲区，复制数据上去，然后通过命令把gpu对应的地址和指令写入
----最终也是通过围栏来控制缓冲区是否已经使用完毕的

--DynamicUploadBuffer
--动态的创建一个上传缓冲区


!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!关于上传缓冲区。我们可以看到d3d12book的实现流程!!!!
--1. 创建一个默认缓冲区
--2. 创建一个上传缓冲区
--3. 把传入的数据导入上传缓冲区，再转到默认缓冲区，采用了api:UpdateSubresources
而在miniEngine中可以看到也提供了类似的接口
--1. 创建默认缓冲区 DescriptorAllocator有实现，不过没有对外暴露函数
--2. 创建上传缓冲区 LinearAllocator
--3. 数据->上传缓冲区->默认缓冲区  这里提供了俩方法
----1) CommandContext::WriteBuffer  写入给定的资源中，这个跟d3d12book一致
--------通过返回的资源创建对应视图
--------然后再通过GraphicsContext::SetIndexBuffer\SetVertexBuffer设置顶点缓冲和索引缓冲
----2）GraphicsContext::SetDynamicVB\SetDynamicIB 直接设置顶点缓冲和索引缓冲，和上边的区别就是没有保存默认缓冲区
--------那么这种方式可行吗？
--------如果可行那就代表上传缓冲区对于CPU和GPU来说都是可以访问的，只不过相对的内存地址不同，DynAlloc管理这俩地址
--------经过测试是可行的，详见代码：
