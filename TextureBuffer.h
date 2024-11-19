#pragma once

#include "d3dUtil.h"
class TextureBuffer
{
public:
	//构造
	TextureBuffer(ID3D12Device* device,  UINT objCount):objCount(objCount)
	{
		//使用默认的图片缓冲（这里得到一下size就可以开始偏移了）
		D3D12_RESOURCE_DESC textureDesc;
		int imageBytesPerRow;
		imageSize = d3dUtil::LoadImageDataFromFile(&imageData, textureDesc, L"C:\\Users\\qaze6\\Pictures\\wall.jpg", imageBytesPerRow);
		

		CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&mDefaultBuffer)));
		//其实至此就创建完成了	
	}

	ID3D12Resource* Resource()const
	{
		return mDefaultBuffer.Get();
	}
	int GetSize()const
	{
		return imageSize;
	}
	void CopyData(ID3D12Device* device,ID3D12CommandQueue* commandQueue,ID3D12GraphicsCommandList* commandList,LPCWSTR filename)
	{
		//把原来的缓冲区给释放掉
		if (mUploadBuffer != nullptr) {
			mUploadBuffer->Release();
			mUploadBuffer = nullptr;
		}
		if (mDefaultBuffer != nullptr)
		{
			mDefaultBuffer->Release();
			mDefaultBuffer = nullptr;
		}

		CD3DX12_RESOURCE_DESC textureDesc;
		int imageBytesPerRow;
		imageSize = d3dUtil::LoadImageDataFromFile(&imageData, textureDesc, filename, imageBytesPerRow);

		CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&mDefaultBuffer)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mDefaultBuffer.Get(), 0, 1);

		CD3DX12_HEAP_PROPERTIES heapProperties2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
  		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties2,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &imageData[0];
		textureData.RowPitch = imageBytesPerRow;
		textureData.SlicePitch = imageBytesPerRow * textureDesc.Height;

		auto resBarrier0 = CD3DX12_RESOURCE_BARRIER::Transition(mDefaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST);

		commandList->ResourceBarrier(1, &resBarrier0);

		UpdateSubresources(commandList, mDefaultBuffer.Get(), mUploadBuffer.Get(), 0, 0, objCount, &textureData);
		CD3DX12_RESOURCE_BARRIER resBarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(mDefaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &resBarrier1);

		ID3D12CommandList* cmdsLists[] = { commandList };
		commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	}
private:
	ComPtr<ID3D12Resource> mDefaultBuffer;
	ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* imageData = nullptr;

	UINT objCount;
	int imageSize;
};

