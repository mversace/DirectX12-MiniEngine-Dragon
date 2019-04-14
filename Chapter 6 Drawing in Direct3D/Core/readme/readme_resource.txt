
接口说明：
--资源：ID3D12Resource
--描述符句柄：D3D12_CPU_DESCRIPTOR_HANDLE

文件说明：
--EsramAllocator
--无用类，也许是微软暂时还没有实现

--GpuResource
--对ID3D12Resource的简单封装

--PixelBuffer -> GpuResource
--像素缓冲区
--对于资源来说，很多就是gpu中的一块内存，可以叫buff、缓冲区等
--这里实现的就是像素缓冲区，规定该buff个结构是像素类型，规定了每个像素的格式

--ColorBuffer -> PixelBuffer
--颜色缓冲区
--进一步规定了每个像素的结构是颜色格式
--还维护有3种描述符句柄：
----m_SRVHandle: 着色器资源视图句柄
----m_RTVHandle: 渲染目标视图	句柄		！！通过Create创建的缓冲区才会创建该视图
----m_UAVHandle[12]: 无序访问视图句柄		！！通过Create创建的缓冲区才会创建该视图

--DepthBuffer -> PixelBuffer
--深度/模板缓冲区
--维护有3种描述符句柄：
----m_hDSV[4]: 4种不同意义的深度视图句柄
----m_hDepthSRV: 深度着色器资源视图句柄
----m_hStencilSRV: 模板着色器资源视图句柄