
接口说明：
--描述符堆：ID3D12DescriptorHeap
--描述符句柄：D3D12_CPU_DESCRIPTOR_HANDLE

文件说明：
--DescriptorHeap
--描述符堆管理池，需要初始化为一种特定类型，可以分配出对应的描述符堆

--DynamicDescriptorHeap
--动态的描述符管理器

当GPU操作资源(ID3D12Resource)时，需要知道该资源是什么格式
这就需要描述符来指定，也就是D3D12_CPU_DESCRIPTOR_HANDLE指定该资源的格式信息

描述符堆就是用于管理描述符句柄的，一个类型的描述符堆可以生成对应的描述符
而描述符堆管理池，可以生成新的描述符堆，本文件每个池子默认支持256个堆，超过了就再次申请256个描述符堆