#pragma once

#include "d3dUtil.h"
class TextureBuffer
{
public:
	//构造

	//这里拷贝的话直接用要拷贝的Resource 然后CopyResource进去就行了（cmdList->CopyResource(mBlurMap0.Get(), input);）就像这样，下面的0 是存储用的可以当作SRV 下面的1是用来读写的UAV，然后交换使用(SRV->UAV UAV -> SRV)，不过DSV只需要用到一个就行了
private:
	CD3DX12_CPU_DESCRIPTOR_HANDLE mTexBuffer0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mTexBuffer0CPUUav;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mTexBuffer1CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mTexBuffer1CpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mTexBuffer0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mTexBuffer0GpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mTexBuffer1GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mTexBuffer1GpuUav;

	ComPtr<ID3D12Resource> mtexBuffer0;
	ComPtr<ID3D12Resource> mtexBuffer1;
};

