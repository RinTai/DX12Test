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
}

FrameResource::~FrameResource()
{

}