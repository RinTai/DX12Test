#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount,UINT materialCount,UINT width , UINT height)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
    PassCBPixelized = std::make_unique<UploadBuffer< PassConstantsPixel>>(device, passCount, true);

    std::unique_ptr<TextureBuffer> ColorTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    std::unique_ptr<TextureBuffer> PositionTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);
    std::unique_ptr<TextureBuffer> NormalTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    std::unique_ptr<TextureBuffer> DepthTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_D32_FLOAT);

    BuildGbufferHeapsAndRTV(device);
}

FrameResource::~FrameResource()
{

}

/// <summary>
/// 存储4个RTV的Heap位置，然后在OMSetRenderTarget中获得结果
/// </summary>
void FrameResource::BuildGbufferHeapsAndRTV(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC GbufferHeapDesc;
    GbufferHeapDesc.NumDescriptors = 4;
    GbufferHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    GbufferHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    GbufferHeapDesc.NodeMask = 0;
    ThrowIfFailed(device->CreateDescriptorHeap(&GbufferHeapDesc, IID_PPV_ARGS(mGbufferRtvHeap.GetAddressOf())));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mGbufferRtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //其实也可以用Buffer里的（这个第一个指针是空的，为啥）
    device->CreateRenderTargetView(ColorTexture->Resource(),nullptr, rtvHandle); //颜色
    GbRtvHandles[0] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(PositionTexture->Resource(), nullptr, rtvHandle); //位置
    GbRtvHandles[1] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(NormalTexture->Resource(), nullptr, rtvHandle); //法线
    GbRtvHandles[2] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(DepthTexture->Resource(), nullptr, rtvHandle); //深度
    GbRtvHandles[3] = rtvHandle;
  
    
}