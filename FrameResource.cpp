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

    ColorTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    PositionTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);
    NormalTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    //这个深度纹理注意一下，不是用作绘制的，是用来重建这些的。所以我们用RGB 而不是 D32来解决.. DSV在dxApp的DSV里面。
    //Direct3D 12 不允许直接通过着色器访问或修改绑定为 DSV 的纹理。这是为了优化性能和确保硬件的深度测试过程高效。
    DepthTexture = std::make_unique<TextureBuffer>(device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);

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
    ThrowIfFailed(device->CreateDescriptorHeap(&GbufferHeapDesc, IID_PPV_ARGS(&mGbufferRtvHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mGbufferRtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //其实也可以用Buffer里的
    device->CreateRenderTargetView(ColorTexture->Resource(),nullptr, rtvHandle); //颜色
    GbRtvHandles[0] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(PositionTexture->Resource(), nullptr, rtvHandle); //位置
    GbRtvHandles[1] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    device->CreateRenderTargetView(NormalTexture->Resource(), nullptr, rtvHandle); //法线
    GbRtvHandles[2] = rtvHandle;
    rtvHandle.ptr += rtvDescriptorSize;

    //深度这里有问题
    device->CreateRenderTargetView(DepthTexture->Resource(), nullptr, rtvHandle); //深度
    GbRtvHandles[3] = rtvHandle;
  
    
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GbufferView()const
{
    return GbRtvHandles[4];
}