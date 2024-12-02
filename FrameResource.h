#pragma once

#include "d3dUtil.h"
#include "UploadBuffer.h" 
#include "TextureBuffer.h"

struct ObjectConstants
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT     MaterialIndex;
    UINT     ObjPad0;
    UINT     ObjPad1;
    UINT     ObjPad2;
};

struct InstanceData
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex;
    UINT InstancePad0;
    UINT InstancePad1;
    UINT InstancePad2;
};

struct PassConstantsPixel
{

    XMFLOAT4X4 M;
    XMFLOAT4X4 V;
    XMFLOAT4X4 P;
    XMFLOAT4X4 MVP;
    XMFLOAT4X4 VP;

    XMFLOAT4X4 PixelSpaceMat;
    XMFLOAT4X4 InvPixelSpaceMat;
    XMFLOAT4X4 N2S;

    XMFLOAT4 agentCameraPos;

    float PixelSize;
    XMFLOAT3 Padding3;
};
struct PassConstants
{
    XMFLOAT4X4 M;
    XMFLOAT4X4 V;
    XMFLOAT4X4 P;
    XMFLOAT4X4 MVP;
    XMFLOAT4X4 VP;

    XMFLOAT3 CameraPosition;
    float cbPerObjectPad1 = 0.0f;

    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
    XMFLOAT2 cbPerObjectPad2;

    XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT2 cbPerObjectPad3;
    Light Lights[MaxLights];
    /*DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    */
};
/*struct Vertex
{
    Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) : position(x, y, z), normal(nx, ny, nz), texCoord(u, v) {}
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};*/
struct MaterialData
{
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 64.0f;

    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    UINT DiffuseMapIndex = 0;
    UINT MaterialPad0;
    UINT MaterialPad1;
    UINT MaterialPad2;
};
struct Vertex
{
    Vertex() = default;
    Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
        position(x, y, z),
        normal(nx, ny, nz),
        texCoord(u, v) {}

    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
    XMFLOAT4 color;
};


struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount,UINT materialCount,UINT width,UINT height);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    //每一帧自己的命令分配器
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    //每一帧的常量缓冲区
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstantsPixel>> PassCBPixelized = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
    //Gbuffer的纹理资源
    std::unique_ptr<TextureBuffer> ColorTexture = nullptr;
    std::unique_ptr<TextureBuffer> PositionTexture = nullptr;
    std::unique_ptr<TextureBuffer> NormalTexture = nullptr;
    std::unique_ptr<TextureBuffer> DepthTexture = nullptr;
    
    //Gbuffer的Heap(这里可以调整一下，把RTVHandle 放进TextureBuffer里存储)
    ComPtr<ID3D12DescriptorHeap> mGbufferSrvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> mGbufferRtvHeap = nullptr;
    CD3DX12_CPU_DESCRIPTOR_HANDLE GbRtvHandles[4]; //每个纹理在描述符堆中的位置，颜色 位置 法线 深度

    //把命令标记到这个围栏点
    UINT64 Fence = 0;
private:
    void BuildGbufferHeapsAndRTV(ID3D12Device* device);
};

