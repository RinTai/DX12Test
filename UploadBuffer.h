#pragma once
#include"d3dUtil.h"

template<typename T>
class UploadBuffer
{
public:
    //构造函数
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
    {
        mElementByteSize = sizeof(T);

        mDevice = device;//11.5
        if (isConstantBuffer)
            mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));


        CD3DX12_RESOURCE_DESC tempUse2 = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
        CD3DX12_HEAP_PROPERTIES tempUse(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(
            &tempUse,
            D3D12_HEAP_FLAG_NONE,
            &tempUse2,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mUploadBuffer)));

        ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer()
    {
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);

        mMappedData = nullptr;
    }

    ID3D12Resource* Resource()const
    {
        return mUploadBuffer.Get();
    }

    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
    }

    ID3D12Device* Device()
    {
        return mDevice;
    }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;
    ID3D12Device* mDevice = nullptr;//11.5
    UINT mElementByteSize = 0;
    bool mIsConstantBuffer = false;
};