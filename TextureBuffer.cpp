#include "TextureBuffer.h"

ID3D12Resource *TextureBuffer::Resource()const
{
	return mtexBuffer0.Get();
}

void TextureBuffer::OnResize(UINT newWidth, UINT newHeight)
{
	mWidth = newWidth;
	mHeight = newHeight;

	BuildResource();

	BuildDescriptor();
}

void TextureBuffer::BuildDescriptor(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize)
{
	//这里的位置就在调用的类里分配
	mTexBuffer0CpuSrv = hCpuDescriptor;
	mTexBuffer0CpuUav = hCpuDescriptor.Offset(1,descriptorSize);
	mTexBuffer1CpuSrv = hCpuDescriptor.Offset(1, descriptorSize);
	mTexBuffer1CpuUav = hCpuDescriptor.Offset(1, descriptorSize);

	mTexBuffer0GpuSrv = hGpuDescriptor;
	mTexBuffer0GpuUav = hGpuDescriptor.Offset(1, descriptorSize);
	mTexBuffer1GpuSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mTexBuffer1GpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptor();
}

void TextureBuffer::BuildDescriptor()
{
	//给每个SRV UAV豆分配内存
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	mDevice->CreateShaderResourceView(mtexBuffer0.Get(), &srvDesc, mTexBuffer0CpuSrv);
	mDevice->CreateUnorderedAccessView(mtexBuffer0.Get(),nullptr, &uavDesc, mTexBuffer0CpuUav);

	mDevice->CreateShaderResourceView(mtexBuffer1.Get(), &srvDesc, mTexBuffer1CpuSrv);
	mDevice->CreateUnorderedAccessView(mtexBuffer1.Get(), nullptr, &uavDesc, mTexBuffer1CpuUav);
}

void TextureBuffer::BuildResource()
{
	//纹理格式这些
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_HEAP_PROPERTIES temp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(
		&temp,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mtexBuffer0)));

	ThrowIfFailed(mDevice->CreateCommittedResource(
		&temp,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mtexBuffer1)));
	
	
}