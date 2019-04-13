
接口说明：
--命令队列：ID3D12CommandQueue
--命令列表：ID3D12CommandList
--命令分配器：ID3D12CommandAllocator
--围栏：ID3D12Fence

文件说明：
--CommandAllocatorPool
--命令分配器池，需要初始化为一种特定类型。通过围栏机制可以做到分配器复用

--CommandListManager
--维护命令队列、命令列表、围栏。

--CommandContext
--对命令的整体封装，方便使用

控制GPU执行命令流程:
1. 假设已经有了ID3D12Device

2. 生成一个围栏ID3D12Fence
	ID3D12Device->CreateFence

3. 创建针对该设备的命令队列: ID3D12CommandQueue
	ID3D12Device->CreateCommandQueue

4. 创建一个命令分配器:ID3D12CommandAllocator（对应你所要执行的命令类型）
	ID3D12Device->CreateCommandAllocator

5. 使用该命令分配器生成一个命令列表: ID3D12CommandList
	ID3D12Device->CreateCommandList

6. 向命令列表中插入命令
	ID3D12CommandList->xxx		// 插入命令
	ID3D12CommandList->xxx		// 插入命令
	CreateCommandList->close(); // 关闭

7. 发送给GPU执行命令
	ID3D12CommandQueue->ExecuteCommandLists

8. 插入围栏值
	ID3D12CommandQueue-Signal

9. 其他操作，交换缓冲区等，不属于这里的功能

说明如下：
1. 步骤3、4、5是必备的几个东西。
2. 步骤6，实际是把命令插入了命令分配器中
3. 步骤7，仅仅是告诉GPU开始执行，GPU会读取命令分配器中的命令逐条执行
4. 步骤8，因为GPU维护的是一个队列（环形队列），只有在执行完上边的命令后才会执行到这个围栏
	执行到这个围栏时，会把这里设置的围栏值更新到围栏对象中，使得围栏对象可以知道步骤7的命令是否执行完