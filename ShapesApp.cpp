//***************************************************************************************
// 目前还没有解决纹理赋值的问题，只是把材质缓冲区赋值上去了（已经解决了）注意一下SRV UAV 一个是输入一个是输出
// 目前只采用了前向渲染Forward 可以试试延迟渲染，创建多个RTV。
// 
// （目前的材质是和物体绑定在一起的）将材质与shader绑定在一起的方法，为每个Mat或者Ritem存储一个指向PSO的索引，同时为每种PSO设置一个根签名控制输入(像untiy)
// 合批的使用猜想，顶点缓冲区必须分成静态动态的，静态的话，可以为RITEM 加入一个 STATIC的 bool值，在每个Material的 hash表中更新顶点以达到合批，有点麻烦.动态的话就
// 在每一帧中，你需要遍历所有动态物体，收集它们的顶点和索引数据，并合并
// 
// (在RTV绑定了DSV的情况下，GPU会自动的把每个片元的深度信息写入到深度缓冲区，不然就得自己写了)
// 阴影贴图和级联阴影: 单独新写一个compute shader，传入光源的阴影矩阵和当前顶点，经过光源的DSV比较后判断？渲染场景时，在光源的视角下，设置阴影的DSV为输出目标，得到光源的DSV
// 然后在与主Camera中的DSV比较（转到光源空间下，再拿到纹理坐标），来判断是否被遮挡（其实不需要贴图，做一个判断）。
// 
// 动态索引编译成功了，但是不知道纹理为什么还是不能赋值上去？是内存分配出问题了吗？ - 11.3
// 
// 想做成延迟渲染，或者只是绘制阴影图？感觉工作量好大
// 添加后处理效果中，是屏幕像素化的后处理，感觉还是得自己写管线 -11.19 
//***************************************************************************************
#include "d3dUtil.h"
#include "dxApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "FrameResource.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct RenderItem
{
    //表示需要提供给流水线的PSO的数据集

    RenderItem() = default;

    //描述物体空间相对于时间空间的世界矩阵
    //定义物体的位置朝向和大小
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();//

    //使用 DIRTY FLAG 表示物体的相关数据已经发生改变，所以对每个FrameResource进行更新。Count就等于如下
    int  NumFramesDirty = gNumFrameResources;

    //这个索引指向GPU常量缓冲区对应与当前渲染项的物体常量缓冲区
    UINT ObjCBIndex = -1;

    bool Visible = true;

    MeshGeometry* Geo = nullptr;
    Material* Mat = nullptr;
    //图元拓扑
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //AABB盒用于剔除
    BoundingBox Bounds;
    //实例化数据
    vector<InstanceData> Instances;
    //DrawIndexInstanced 的参数
    UINT IndexCount = 0;
    UINT InstanceCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

    //每个Ritem的结构化缓冲区
    unique_ptr<UploadBuffer<InstanceData>> InstanceBuffer = nullptr;

    void InitialBuffer(ID3D12Device* device)
    {
        InstanceBuffer = make_unique<UploadBuffer<InstanceData>>(device,InstanceCount,false);
    }

    //11.5感觉会报错
    RenderItem& operator=(const RenderItem& other)
    {
        if (this == &other) return *this;
        else
        {
            World = other.World;
            TexTransform = other.TexTransform;
            NumFramesDirty = other.NumFramesDirty;
            ObjCBIndex = other.ObjCBIndex;
            Geo = other.Geo;
            Mat = other.Mat;
            PrimitiveType = other.PrimitiveType;
            Bounds = other.Bounds;
            Instances = other.Instances;
            IndexCount = other.IndexCount;
            InstanceCount = other.InstanceCount;
            StartIndexLocation = other.StartIndexLocation;
            BaseVertexLocation = other.BaseVertexLocation;

            if (other.InstanceBuffer)
            {
                InstanceBuffer = make_unique<UploadBuffer<InstanceData>>(other.InstanceBuffer->Device(), InstanceCount, false);

                auto& instanceData = other.Instances;
                for (UINT i = 0; i < other.InstanceCount; i++)
                {
                    XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
                    XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

                    InstanceData data;
                    XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
                    XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
                    data.MaterialIndex = instanceData[i].MaterialIndex;
                    InstanceBuffer->CopyData(i, data);
                }
            }
        }
    }
};

enum class RenderLayer : int
{
    Opaque = 0,
    Mirrors,
    Reflected,
    Transparent,
    Shadow,
    Highlight,
    Count
};
class ShapesApp : public DxApp
{
public:
    ShapesApp(HINSTANCE hInstance);
    ShapesApp(const ShapesApp& rhs) = delete;
    ShapesApp& operator=(const ShapesApp& rhs) = delete;
    ~ShapesApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
    

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void LoadTexture();

    void OnKeyboardInput(const GameTimer& gt);
    void UpdateCamera(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateInstanceData(const GameTimer& gt);
    void UpdateMaterialBuffers(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateReflectedPassCB(const GameTimer& gt);
    void UpdatePixelizedPassCB(const GameTimer& gt);//11.18这个是更新自己写那个后处理效果的，我在想也没有更统一的方式，当然后处理的实现，还是那个三角形那个，太天才了（我还没做，延迟处理阶段还没弄完呢）

    void BuildGbufferPSOs();
    void BuildGbufferRootSignature();

    void BuildDescriptorHeaps();
    void BuildConstantBufferViews();
    void BuildMaterial();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildRoomGeometry();
    void BuildShapeGeometry();
    void BuildSkullGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

    void Pick(int sx, int sy);

    //Test

    array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    //GB的Signature
    ComPtr<ID3D12RootSignature> mGBSignature = nullptr;
    //或者整成Unordered_map ?
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr; 
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
    //srv应该用一个unordered map 每个纹理都有一个SRV(只是放纹理的)//11.24
    ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;


    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map <std::string, std::unique_ptr<Texture>> mTextures; //仍未使用  10.3
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;//后处理效果也写里面，因为每一个都要创建一个PSO，做成栈也取不出来..可能会更麻烦。
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;


    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    //混合设置
    D3D12_BLEND_DESC mBlendDesc;


    RenderItem* mSkullRitem = nullptr;
    RenderItem* mReflectedSkullRitem = nullptr;
    RenderItem* mShadowedSkullRitem = nullptr;
    RenderItem* mPickedRitem = nullptr;
    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.

    std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

    PassConstants mMainPassCB;
    PassConstants mReflectedPassCB;

    UINT mPassCbvOffset = 0;
    //摄像机视锥体
    bool mCameraFrustumEnable = true;
    BoundingFrustum mCameraFrustum;
    
    bool mIsWireframe = false;

    XMFLOAT3 mSkullTranslation = { 0.0f,1.0f,-5.0f };
    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = 0.2f * XM_PI;
    float mRadius = 15.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd){
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    
    try
    {
        ShapesApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
    : DxApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
    if (mDevice != nullptr)
        FlushCommandQueue();
}

/// <summary>
/// 初始化
/// </summary>
/// <returns></returns>
bool ShapesApp::Initialize()
{
    if (!DxApp::Initialize())
        return false;

    mCamera.SetPosition(0.0f, 2.0f, -15.0f);
    // 重置命令
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    LoadTexture();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    //BuildShapeGeometry();
    BuildRoomGeometry();
    BuildSkullGeometry();
    BuildMaterial();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    BuildConstantBufferViews();
    BuildPSOs();

    //执行命令
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 等待CPUGPU同步
    FlushCommandQueue();

    return true;
}

void ShapesApp::OnResize()
{
    DxApp::OnResize();

    // 缩放时更新投影矩阵

     mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

     //视锥体计算（法1，用八个角点，投影矩阵的逆矩阵，在NDC空间下逆推，法2 用斜率与原点推，斜率比如右平面法线为（Rigthslope,0,-1）加原点，就能得到了）
    BoundingFrustum::CreateFromMatrix(mCameraFrustum, mCamera.GetProj());
}

void ShapesApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    //UpdateCamera(gt);

  
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr,nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    //-----------------------------------//更新CB
    UpdateObjectCBs(gt);
    UpdateInstanceData(gt);
    UpdateMainPassCB(gt);
    UpdateMaterialBuffers(gt);
    UpdateReflectedPassCB(gt);
}
/// <summary>
/// 绘制
/// </summary>
/// <param name="gt"></param>
void ShapesApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());

    //开启线框模式和面模式
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    //重新设置视口这些
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);
    //资源屏障 用于资源转换 （当前后台缓冲区，从呈现状态 到 渲染目标状态，就可以修改了)还有两个子资源 
    D3D12_RESOURCE_BARRIER tempUse = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &tempUse);

    D3D12_CPU_DESCRIPTOR_HANDLE tempDsv = DepthStencilView();
    D3D12_CPU_DESCRIPTOR_HANDLE tempRtv = CurrentBackBufferView();

    //清理深度缓冲区和RTV
    mCommandList->ClearRenderTargetView(tempRtv, Colors::LightBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(tempDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // 选择绘制的缓冲区
    
    mCommandList->OMSetRenderTargets(1, &tempRtv, true, &tempDsv);

    ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    //int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
    //auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    //passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
    //mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    //使用根描述符的CBV 最新的
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
    mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

    mCommandList->SetGraphicsRootDescriptorTable(3, mSrvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["highlight"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Highlight]);
    //把模板缓冲区标记为1
    mCommandList->OMSetStencilRef(1);
    mCommandList->SetPipelineState(mPSOs["markStencilMirrors"].Get());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Mirrors]);
    //只绘制镜子范围的镜像（标记为1的）
    // 使用两个单独的渲染过程常量缓冲区，一个存储物体镜像，另一个保存光照镜像（这个镜子的实现实际上是画了两个骷髅头，用模板测试的方法来剔除一部分）
    
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress() + 1 * passCBByteSize);
    mCommandList->SetPipelineState(mPSOs["drawStencilReflections"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Reflected]);

    //恢复主渲染过程
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    mCommandList->OMSetStencilRef(0);

    //绘制镜面，使他与镜像混合
    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    //绘制阴影
    mCommandList->SetPipelineState(mPSOs["shadow"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Shadow]);


    //从后台显示到前面
    D3D12_RESOURCE_BARRIER tempUse2(CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mCommandList->ResourceBarrier(1, &tempUse2);

    // 关闭
    ThrowIfFailed(mCommandList->Close());

    //添加到命令队列中执行
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 交换交换链
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // 增加围栏值，标记到此围栏点上
    mCurrFrameResource->Fence = ++mCurrentFence;

    //向队列里添加一条指令，设置新的围栏点，G等待GPU处理完gSignal之前的所有命令
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        mLastMousePos.x = x;
        mLastMousePos.y = y;

        SetCapture(mhMainWnd);
    }
    else if ((btnState & MK_MBUTTON) != 0)
    {
        Pick(x, y);
    }
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    lastMousePos.x = x;
    lastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = 0.01f;
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;

    if (GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(5.0f * dt);

    if (GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-5.0f * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-5.0f * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(5.0f * dt);

    if (GetAsyncKeyState('1') & 0x8000)
        mCameraFrustumEnable = true;

    if (GetAsyncKeyState('2') & 0x8000)
        mCameraFrustumEnable = false;
    mCamera.UpdateViewMatrix();

    // Don't let user move below ground plane.
    mSkullTranslation.y = MathHelper::Max(mSkullTranslation.y, 0.0f);

    // Update the new world matrix.
    XMMATRIX skullRotate = XMMatrixRotationY(0.5f * MathHelper::Pi);
    XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
    XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
    XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
    XMStoreFloat4x4(&mSkullRitem->World, skullWorld);

    // Update reflection world matrix. 反射的矩阵
    XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
    XMMATRIX R = XMMatrixReflect(mirrorPlane);
    XMStoreFloat4x4(&mReflectedSkullRitem->World, skullWorld * R);

    // Update shadow world matrix. 阴影的矩阵 （优化方法:阴影贴图）
    XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
    XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
    XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
    //避免由于深度缓冲区精度问题而导致的阴影自我遮挡（self-shadowing）问题
    XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
    XMStoreFloat4x4(&mShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);

    mSkullRitem->NumFramesDirty = gNumFrameResources;
    mReflectedSkullRitem->NumFramesDirty = gNumFrameResources;
    mShadowedSkullRitem->NumFramesDirty = gNumFrameResources;
}

//更新摄像机 咋感觉不用呢
void ShapesApp::UpdateCamera(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
    mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
    mEyePos.y = mRadius * cosf(mPhi);

    // 建造V矩阵
    XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        //更新物体的常量缓冲区
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.MaterialIndex = e->Mat->MatCBIndex;

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);///11.5

            // Next FrameResource need to be updated too.
            e->NumFramesDirty--;
        }
    }
}
void ShapesApp::UpdateInstanceData(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    //这是IV，
    XMVECTOR tempD = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&tempD, view);


    for (auto& e : mAllRitems)
    {
        //更新每个渲染物体的常量缓冲区
        const auto& instanceData = e->Instances;

        auto& instanceBuffer = e->InstanceBuffer;
        UINT visibleInstance = 0;

        int visibleInstanceCount = 0;
        for (UINT i = 0; i < e->Instances.size(); i++)
        {

            XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            //INV VM矩阵
            XMVECTOR tempW = XMMatrixDeterminant(world);
            XMMATRIX invWorld = XMMatrixInverse(&tempW, world);
            XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);
            
            BoundingFrustum localSpaceFrustum;
            mCameraFrustum.Transform(localSpaceFrustum, viewToLocal);

            if ((localSpaceFrustum.Contains(e->Bounds) != DISJOINT) || (mCameraFrustumEnable != false))
            {
                InstanceData data;
                XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
                XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
                data.MaterialIndex = instanceData[i].MaterialIndex;


                instanceBuffer->CopyData(visibleInstanceCount++, data);
            }

        }

        e->InstanceCount = visibleInstanceCount;    
    }
}
//10.31
void ShapesApp::UpdateMaterialBuffers(const GameTimer& gt)
{
    auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialData matData;
            matData.DiffuseAlbedo = mat->DiffuseAlbedo;
            matData.FresnelR0 = mat->FresnelR0;
            matData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    /*XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMVECTOR tempV = XMMatrixDeterminant(view);
    XMMATRIX invView = XMMatrixInverse(&tempV,view);
    XMVECTOR tempP = XMMatrixDeterminant(proj);
    XMMATRIX invProj = XMMatrixInverse(&tempP, proj);
    XMVECTOR tempVP = XMMatrixDeterminant(viewProj);
    XMMATRIX invViewProj = XMMatrixInverse(&tempVP, viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();*/

    XMMATRIX v = mCamera.GetView();
    XMMATRIX p = mCamera.GetProj();
    XMMATRIX m = XMMatrixIdentity();
    XMMATRIX VP = v * p;
    XMMATRIX MVP = m * v * p;

    XMStoreFloat4x4(&mMainPassCB.M, XMMatrixTranspose(m));
    XMStoreFloat4x4(&mMainPassCB.V, XMMatrixTranspose(v));
    XMStoreFloat4x4(&mMainPassCB.P, XMMatrixTranspose(p));
    XMStoreFloat4x4(&mMainPassCB.VP, XMMatrixTranspose(VP));
    XMStoreFloat4x4(&mMainPassCB.MVP, XMMatrixTranspose(MVP));
    mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.CameraPosition = mCamera.GetPosition();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

/// <summary>
/// 至于为什么要在这里多家一个常量缓冲区，
/// </summary>
/// <param name="gt"></param>
void ShapesApp::UpdateReflectedPassCB(const GameTimer& gt)
{
    mReflectedPassCB = mMainPassCB;

    XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
    XMMATRIX R = XMMatrixReflect(mirrorPlane);

    // 反射光
    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR lightDir = XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
        XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLightDir);
    }

    // 把光照镜像的渲染过程常量数据存于渲染过程缓冲区索引1的位置
    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(1, mReflectedPassCB);
}

//这里修改了一下来使用SRV
void ShapesApp::BuildDescriptorHeaps()
{
    UINT objCount =  (UINT)mRitemLayer[(int)RenderLayer::Opaque].size();

    //需要为每一个帧资源中的每一个物体创建一个CBV描述符加上渲染过程Pass的CBV就+1(添加一个SRV)
    UINT numDescriptors = (objCount  + 1) * gNumFrameResources;

    // 保存给Pass描述符堆的偏移，现在是最后面的三个描述符
    mPassCbvOffset =  objCount * gNumFrameResources;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = numDescriptors;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc,IID_PPV_ARGS(&mCbvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 4;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

    auto bricksTex = mTextures["bricksTex"]->Resource;
    auto checkboardTex = mTextures["checkboardTex"]->Resource;
    auto iceTex = mTextures["iceTex"]->Resource;
    auto white1x1Tex = mTextures["white1x1Tex"]->Resource;

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = bricksTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    mDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);
    
    // 下一张纹理的描述符
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    srvDesc.Format = checkboardTex->GetDesc().Format;
    mDevice->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

    // 下一张纹理的描述符
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    srvDesc.Format = iceTex->GetDesc().Format;
    mDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

    // 下一张纹理的描述符
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    srvDesc.Format = white1x1Tex->GetDesc().Format;
    mDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);

    //如果纹理多的话Offset 1 size 再创建
}

/// <summary>
/// 灵活性高：描述符堆可以包含多种类型的描述符（如CBV、SRV、UAV、RTV等），适合管理大量资源 但是我们采样根描述符 直接访问：根描述符直接指向GPU虚拟地址，允许快速访问资源。简单高效：设置根描述符比描述符堆更简单，适合绑定单个资源。
/// </summary>
void ShapesApp::BuildConstantBufferViews()
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    UINT objCount = (UINT)mRitemLayer[(int)RenderLayer::Opaque].size();

    // 每一个帧资源的每一个物体都要有一个对应的CBV
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        //获得每个帧资源的Resource 可以用来得到起始位置
        auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
        for (UINT i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

            // 偏移到缓冲区第i给物体的常量缓冲区
            cbAddress +=  i * objCBByteSize;

            // 偏移到该物体在描述符堆中的CBV  前面的3n
            int heapIndex = frameIndex * objCount +  i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            //创建视图
            mDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    //最后3个描述符是每个帧资源的渲染过程CBV
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {

        auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
        // 每个帧资源的渲染过程缓冲区中只存在一个常量缓冲区
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

       //偏移 3(n+1)最后的3个
        int heapIndex = mPassCbvOffset + frameIndex;
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = passCBByteSize;

        mDevice->CreateConstantBufferView(&cbvDesc, handle);
    }       
}
void ShapesApp::LoadTexture()
{
    auto bricksTex = std::make_unique<Texture>();
    bricksTex->Name = "bricksTex";
    bricksTex->Filename = L"C:\\Users\\qaze6\\Pictures\\bricks3.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), bricksTex->Filename.c_str(),
        bricksTex->Resource, bricksTex->UploadHeap));

    auto checkboardTex = std::make_unique<Texture>();
    checkboardTex->Name = "checkboardTex";
    checkboardTex->Filename = L"C:\\Users\\qaze6\\Pictures\\checkboard.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), checkboardTex->Filename.c_str(),
        checkboardTex->Resource, checkboardTex->UploadHeap));

    auto iceTex = std::make_unique<Texture>();
    iceTex->Name = "iceTex";
    iceTex->Filename = L"C:\\Users\\qaze6\\Pictures\\ice.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), iceTex->Filename.c_str(),
        iceTex->Resource, iceTex->UploadHeap));

    auto white1x1Tex = std::make_unique<Texture>();
    white1x1Tex->Name = "white1x1Tex";
    white1x1Tex->Filename = L"C:\\Users\\qaze6\\Pictures\\white1x1.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), white1x1Tex->Filename.c_str(),
        white1x1Tex->Resource, white1x1Tex->UploadHeap));

    mTextures[bricksTex->Name] = std::move(bricksTex);
    mTextures[checkboardTex->Name] = std::move(checkboardTex);
    mTextures[iceTex->Name] = std::move(iceTex);
    mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
}

void ShapesApp::BuildGbufferRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE gbTable;
    gbTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0);//(t0,space0)

    CD3DX12_ROOT_PARAMETER gbRootParameter[4];//和下面一样的要放进去 但是可以把常量删了
    gbRootParameter[0].InitAsConstantBufferView(1);//PASSCB
    gbRootParameter[1].InitAsShaderResourceView(0, 1);//Mat(t0,space1)
    gbRootParameter[2].InitAsShaderResourceView(1, 1);//Instance(t1,space1)
    gbRootParameter[3].InitAsDescriptorTable(4, &gbTable, D3D12_SHADER_VISIBILITY_PIXEL);

    //采样器
    auto samplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC gbRootSigDesc(4, gbRootParameter,(UINT)samplers.size(), samplers.data(), 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&gbRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    ThrowIfFailed(mDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mGBSignature.GetAddressOf())));
}

void ShapesApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0); //num register

   /*
    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE srvTable0;
    srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);*/

    // 根参数.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

    slotRootParameter[0].InitAsConstantBufferView(0); //OBJ
    slotRootParameter[1].InitAsConstantBufferView(1); //PASS 
    slotRootParameter[2].InitAsShaderResourceView(0, 1); //MATERIAL
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL); //TEX
    slotRootParameter[4].InitAsShaderResourceView(1 , 1); //INSTANCE

    // 创建CBV的根参数
     //slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
     //slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
     //slotRootParameter[2].InitAsDescriptorTable(1, &srvTable0);

    //采样器
    auto samplers = GetStaticSamplers();

    //中间这个是sampler数量
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,(UINT)samplers.size(), samplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    ThrowIfFailed(mDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void ShapesApp::BuildShadersAndInputLayout()
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif


    //mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
    //mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");
    // 用下面这个会报错，应该是因为complieFlag
    //ThrowIfFailed(D3DCompileFromFile(std::wstring(L"C:\\Users\\qaze6\\source\\repos\\DirectTest\\testShader.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &mShaders["standardVS"], nullptr));
    //ThrowIfFailed(D3DCompileFromFile(std::wstring(L"C:\\Users\\qaze6\\source\\repos\\DirectTest\\testShader.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &mShaders["opaquePS"], nullptr));

    mShaders["standardVS"] = d3dUtil::CompileShader(L"C:\\Users\\qaze6\\source\\repos\\DirectTest\\testShader.hlsl", nullptr, "VSMain", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"C:\\Users\\qaze6\\source\\repos\\DirectTest\\testShader.hlsl", nullptr, "PSMain", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void ShapesApp::BuildRoomGeometry()
{
    // Create and specify geometry.  For this sample we draw a floor
// and a wall with a mirror on it.  We put the floor, wall, and
// mirror geometry in one vertex buffer.
//
//   |--------------|
//   |              |
//   |----|----|----|
//   |Wall|Mirr|Wall|
//   |    | or |    |
//   /--------------/
//  /   Floor      /
// /--------------/

    std::array<Vertex, 20> vertices =
    {
        // Floor: Observe we tile texture coordinates.
        Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
        Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
        Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

        // Wall: Observe we tile texture coordinates, and that we
        // leave a gap in the middle for the mirror.
        Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
        Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

        Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
        Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
        Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

        // Mirror
        Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
        Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
    };

    std::array<std::int16_t, 30> indices =
    {
        // Floor
        0, 1, 2,
        0, 2, 3,

        // Walls
        4, 5, 6,
        4, 6, 7,

        8, 9, 10,
        8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        // Mirror
        16, 17, 18,
        16, 18, 19
    };

    SubmeshGeometry floorSubmesh;
    floorSubmesh.IndexCount = 6;
    floorSubmesh.StartIndexLocation = 0;
    floorSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry wallSubmesh;
    wallSubmesh.IndexCount = 18;
    wallSubmesh.StartIndexLocation = 6;
    wallSubmesh.BaseVertexLocation = 0;

    SubmeshGeometry mirrorSubmesh;
    mirrorSubmesh.IndexCount = 6;
    mirrorSubmesh.StartIndexLocation = 24;
    mirrorSubmesh.BaseVertexLocation = 0;

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "roomGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["floor"] = floorSubmesh;
    geo->DrawArgs["wall"] = wallSubmesh;
    geo->DrawArgs["mirror"] = mirrorSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    //对合并到顶点缓冲区的每个物体的顶点偏移量进行缓存
    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    //
    //对合并索引缓冲区的每个物体的索引偏移量进行缓存
    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    //定义的多个SubmeshGeometry结构体中包含的子网格数据
    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    //
    //提取出所需的顶点元素。然后放进一个顶点缓冲区
    //

    auto totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = box.Vertices[i].Position;
        vertices[k].texCoord = box.Vertices[i].TexC;
        vertices[k].normal = box.Vertices[i].Normal;

        XMVECTOR P = XMLoadFloat3(&vertices[k].position);//11.12
        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    //11.12
    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));


    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = grid.Vertices[i].Position;
        vertices[k].texCoord = grid.Vertices[i].TexC;
        vertices[k].normal = grid.Vertices[i].Normal;
    }


    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = sphere.Vertices[i].Position;
        vertices[k].texCoord = sphere.Vertices[i].TexC;
        vertices[k].normal = sphere.Vertices[i].Normal;
    }


    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = cylinder.Vertices[i].Position;
        vertices[k].texCoord = cylinder.Vertices[i].TexC;
        vertices[k].normal = cylinder.Vertices[i].Normal;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    //把几何数据映射到了Buffer里
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildSkullGeometry()
{
    std::ifstream fin(L"C:\\Users\\qaze6\\Pictures\\skull.txt");

    if (!fin)
    {
        MessageBox(0, L"Models/skull.txt not found.", 0, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
        fin >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;

        XMVECTOR P = XMLoadFloat3(&vertices[i].position);
        //映射到单位圆上
        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        //方位角 横向
        float theta = atan2f(spherePos.z, spherePos.x);
        if (theta < 0)
        {
            theta += XM_2PI;
        }

        //极角，竖向
        float phi = acosf(spherePos.y);

        float u = theta / (2.0f * XM_PI);
        float v = phi / XM_PI;

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);

        // Model does not have texture coordinates, so just zero them out.
        vertices[i].texCoord = { u, v };
    }

    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }

    fin.close();

    //r
    // Pack the indices of all the meshes into one index buffer.
    //

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.Bounds = bounds;

    geo->DrawArgs["skull"] = submesh;

    mGeometries[geo->Name] = std::move(geo);
}

//Gbuffer的PSO
void ShapesApp::BuildGbufferPSOs()
{
    
    //RTV深度在DSV里就画了，但是光源的Shadow还是需要一个PSO的

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gBufferPsoDesc;
    gBufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gBufferPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    gBufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gBufferPsoDesc.SampleMask = UINT_MAX;
    gBufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 绘制三角形

    gBufferPsoDesc.NumRenderTargets = 4;  //法线 位置 颜色 深度
    gBufferPsoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;//法线
    gBufferPsoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;//位置
    gBufferPsoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;//颜色
    gBufferPsoDesc.RTVFormats[3] = DXGI_FORMAT_D32_FLOAT;// 深度
    gBufferPsoDesc.SampleDesc.Count =  1;;  // 单倍样本
    
    D3D12_BLEND_DESC noBlendDesc = {};
    noBlendDesc.AlphaToCoverageEnable = FALSE;
    noBlendDesc.RenderTarget[0].BlendEnable = FALSE;  // 禁用混合
    noBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    noBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    noBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    noBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    noBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    noBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    gBufferPsoDesc.BlendState = noBlendDesc;
    gBufferPsoDesc.DSVFormat = mDepthStencilFormat;

    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&gBufferPsoDesc, IID_PPV_ARGS(&mPSOs["Gbuffer"])));
    
}

void ShapesApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // 不透明物体的PSO.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS = CD3DX12_SHADER_BYTECODE(mShaders["standardVS"].Get());
    /*{
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };*/
    opaquePsoDesc.PS = CD3DX12_SHADER_BYTECODE(mShaders["opaquePS"].Get());
    /*{
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };*/
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; /* D3D12_FILL_MODE_SOLID：以实心方式填充多边形。这是最常用的模式，用于绘制实心的几何体。D3D12_FILL_MODE_WIREFRAME：以线框方式绘制多边形，仅绘制多边形的边缘。 */                                                                                        
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);//混合设置
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // 不透明线框物体的PSO
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

    //
    //  透明物体的PSO
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

    // 
    //  标记镜面范围 的 PSO (把镜面框起来不让越出)
    //
    
    //禁止对渲染目标的写操作
    CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
    mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

    //深度模板的
    D3D12_DEPTH_STENCIL_DESC mirrorDSS;
    mirrorDSS.DepthEnable = true;
    mirrorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    mirrorDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    mirrorDSS.StencilEnable = true;
    mirrorDSS.StencilReadMask = 0xff;
    mirrorDSS.StencilWriteMask = 0xff;

    //通过深度测试就设置为Ref 1（REF是自己设置的）
    mirrorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    //这里是通过模板测试的方法↓
    mirrorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    //我们不渲染后面来着
    mirrorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    mirrorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC markMirrorsPsoDesc = opaquePsoDesc;
    markMirrorsPsoDesc.BlendState = mirrorBlendState;
    markMirrorsPsoDesc.DepthStencilState = mirrorDSS;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&markMirrorsPsoDesc, IID_PPV_ARGS(&mPSOs["markStencilMirrors"])));

    // 
    // 在镜面中渲染物体所用的 PSO
    // 

    D3D12_DEPTH_STENCIL_DESC reflectionsDSS;
    reflectionsDSS.DepthEnable = true;
    reflectionsDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    reflectionsDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    reflectionsDSS.StencilEnable = true;
    reflectionsDSS.StencilReadMask = 0xff;
    reflectionsDSS.StencilWriteMask = 0xff;

    reflectionsDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    reflectionsDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    reflectionsDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    //这里EQUAL的值是我们自己定的
    reflectionsDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

    // 不管
    reflectionsDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    reflectionsDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    reflectionsDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    reflectionsDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionsPsoDesc = opaquePsoDesc;
    drawReflectionsPsoDesc.DepthStencilState = reflectionsDSS;
    drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    drawReflectionsPsoDesc.RasterizerState.FrontCounterClockwise = true;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&drawReflectionsPsoDesc, IID_PPV_ARGS(&mPSOs["drawStencilReflections"])));
    
    // 
    //  阴影的PSO
    //

    D3D12_DEPTH_STENCIL_DESC shadowDSS;
    shadowDSS.DepthEnable = true;
    shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    shadowDSS.StencilEnable = true;
    shadowDSS.StencilReadMask = 0xff;
    shadowDSS.StencilWriteMask = 0xff;

    shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
    shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

    shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
    shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
    shadowPsoDesc.DepthStencilState = shadowDSS;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));


    //高光的PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC highlightPsoDesc = opaquePsoDesc;

    highlightPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    highlightPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&highlightPsoDesc, IID_PPV_ARGS(&mPSOs["highlight"])));
}

void ShapesApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(),
            2, (UINT)mAllRitems.size(),(UINT) mMaterials.size(),(UINT)mClientWidth,(UINT)mClientHeight));
    }
}
void ShapesApp::BuildMaterial()
{
    auto bricks = std::make_unique<Material>();
    bricks->Name = "bricks";
    bricks->MatCBIndex = 0;
    bricks->DiffuseSrvHeapIndex = 0;
    bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    bricks->Roughness = 0.25f;

    auto checkertile = std::make_unique<Material>();
    checkertile->Name = "checkertile";
    checkertile->MatCBIndex = 1;
    checkertile->DiffuseSrvHeapIndex = 1;
    checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
    checkertile->Roughness = 0.3f;

    auto icemirror = std::make_unique<Material>();
    icemirror->Name = "icemirror";
    icemirror->MatCBIndex = 2;
    icemirror->DiffuseSrvHeapIndex = 2;
    icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
    icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    icemirror->Roughness = 0.5f;

    auto skullMat = std::make_unique<Material>();
    skullMat->Name = "skullMat";
    skullMat->MatCBIndex = 3;
    skullMat->DiffuseSrvHeapIndex = 3;
    skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skullMat->Roughness = 0.3f;

    auto shadowMat = std::make_unique<Material>();
    shadowMat->Name = "shadowMat";
    shadowMat->MatCBIndex = 4;
    shadowMat->DiffuseSrvHeapIndex = 3;
    shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
    shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
    shadowMat->Roughness = 0.0f;

    //11.12
    auto highlight0 = std::make_unique<Material>();
    highlight0->Name = "highlight0";
    highlight0->MatCBIndex = 5;
    highlight0->DiffuseSrvHeapIndex = 4;
    highlight0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.6f);
    highlight0->FresnelR0 = XMFLOAT3(0.06f, 0.06f, 0.06f);
    highlight0->Roughness = 0.0f;

    mMaterials["bricks"] = std::move(bricks);
    mMaterials["checkertile"] = std::move(checkertile);
    mMaterials["icemirror"] = std::move(icemirror);
    mMaterials["skullMat"] = std::move(skullMat);
    mMaterials["shadowMat"] = std::move(shadowMat);
    mMaterials["highlight0"] = std::move(highlight0);//11.12
}
void ShapesApp::BuildRenderItems()
{
    auto floorRitem = std::make_unique<RenderItem>();
    floorRitem->World = MathHelper::Identity4x4();
    floorRitem->TexTransform = MathHelper::Identity4x4();
    floorRitem->ObjCBIndex = 0;
    floorRitem->Mat = mMaterials["checkertile"].get();
    floorRitem->Geo = mGeometries["roomGeo"].get();
    floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
    floorRitem->InstanceCount = 1;//11.5
    floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
    floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
    //11.5
    floorRitem->Instances.resize(1);
    floorRitem->Instances[0].World = MathHelper::Identity4x4();;
    floorRitem->Instances[0].TexTransform = floorRitem->TexTransform = MathHelper::Identity4x4();
    floorRitem->Instances[0].MaterialIndex = floorRitem->Mat->MatCBIndex;

    floorRitem->InitialBuffer(mDevice.Get());//11.5
    mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

    auto wallsRitem = std::make_unique<RenderItem>();
    wallsRitem->World = MathHelper::Identity4x4();
    wallsRitem->TexTransform = MathHelper::Identity4x4();
    wallsRitem->ObjCBIndex = 1;
    wallsRitem->Mat = mMaterials["bricks"].get();
    wallsRitem->Geo = mGeometries["roomGeo"].get();
    wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
    wallsRitem->InstanceCount = 1;//11.5
    wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
    wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
    wallsRitem->InitialBuffer(mDevice.Get());//11.5
    //11.5
    wallsRitem->Instances.resize(1);
    wallsRitem->Instances[0].World = MathHelper::Identity4x4();;
    wallsRitem->Instances[0].TexTransform = wallsRitem->TexTransform = MathHelper::Identity4x4();
    wallsRitem->Instances[0].MaterialIndex = wallsRitem->Mat->MatCBIndex;

    mRitemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

    
    auto skullRitem = std::make_unique<RenderItem>();
    skullRitem->World = MathHelper::Identity4x4();
    skullRitem->TexTransform = MathHelper::Identity4x4();
    skullRitem->ObjCBIndex = 2;
    skullRitem->Mat = mMaterials["skullMat"].get();
    skullRitem->Geo = mGeometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
    skullRitem->InstanceCount = 1;//11.5
    skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
    skullRitem->InitialBuffer(mDevice.Get());//11.5
    //11.5
    skullRitem->Instances.resize(1);
    skullRitem->Instances[0].World = MathHelper::Identity4x4();;
    skullRitem->Instances[0].TexTransform = skullRitem->TexTransform = MathHelper::Identity4x4();
    skullRitem->Instances[0].MaterialIndex = skullRitem->Mat->MatCBIndex;

    mSkullRitem = skullRitem.get();
    mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());
    

    // Reflected skull will have different world matrix, so it needs to be its own render item.
    auto reflectedSkullRitem = std::make_unique<RenderItem>();
    *reflectedSkullRitem =  *skullRitem; //这里报错了 11.5
    reflectedSkullRitem->ObjCBIndex = 3;
    mReflectedSkullRitem = reflectedSkullRitem.get();
    mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());

    // Shadowed skull will have different world matrix, so it needs to be its own render item.
    auto shadowedSkullRitem = std::make_unique<RenderItem>();
    *shadowedSkullRitem = *skullRitem;
    shadowedSkullRitem->ObjCBIndex = 4;
    shadowedSkullRitem->Mat = mMaterials["shadowMat"].get();
    mShadowedSkullRitem = shadowedSkullRitem.get();
    mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedSkullRitem.get());

    auto mirrorRitem = std::make_unique<RenderItem>();
    mirrorRitem->World = MathHelper::Identity4x4();
    mirrorRitem->TexTransform = MathHelper::Identity4x4();
    mirrorRitem->ObjCBIndex = 5;
    mirrorRitem->Mat = mMaterials["icemirror"].get();
    mirrorRitem->Geo = mGeometries["roomGeo"].get();
    mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
    mirrorRitem->InstanceCount = 1;//11.5
    mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
    mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
    mirrorRitem->InitialBuffer(mDevice.Get());//11.5
    //11.5
    mirrorRitem->Instances.resize(1);
    mirrorRitem->Instances[0].World = MathHelper::Identity4x4();;
    mirrorRitem->Instances[0].TexTransform = mirrorRitem->TexTransform = MathHelper::Identity4x4();
    mirrorRitem->Instances[0].MaterialIndex = mirrorRitem->Mat->MatCBIndex;

    mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
    mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

    //11.12
    auto pickedRitem = std::make_unique<RenderItem>();
    pickedRitem->World = MathHelper::Identity4x4();
    pickedRitem->TexTransform = MathHelper::Identity4x4();
    pickedRitem->ObjCBIndex = 6;
    pickedRitem->Mat = mMaterials["highlight0"].get();
    pickedRitem->Geo = mGeometries["carGeo"].get();
    pickedRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    pickedRitem->Visible = false;

    pickedRitem->IndexCount = 0;
    pickedRitem->StartIndexLocation = 0;
    pickedRitem->BaseVertexLocation = 0;
    mPickedRitem = pickedRitem.get();
    mRitemLayer[(int)RenderLayer::Highlight].push_back(pickedRitem.get());


    mAllRitems.push_back(std::move(floorRitem));
    mAllRitems.push_back(std::move(wallsRitem));
    mAllRitems.push_back(std::move(skullRitem));
    mAllRitems.push_back(std::move(reflectedSkullRitem));
    mAllRitems.push_back(std::move(shadowedSkullRitem));
    mAllRitems.push_back(std::move(mirrorRitem));
    mAllRitems.push_back(std::move(pickedRitem));//11.12
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    //UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));//11.3

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    //auto matCB = mCurrFrameResource->MaterialBuffer->Resource();//11.3


    // 对每个渲染项.
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        if (ri->Visible == false)
            continue;

        D3D12_VERTEX_BUFFER_VIEW tempVbv = ri->Geo->VertexBufferView();
        D3D12_INDEX_BUFFER_VIEW tempIbv = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &tempVbv);
        cmdList->IASetIndexBuffer(&tempIbv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // 偏移到描述符堆中对应的CBV处
        //UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
        //UINT srvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjSRIndex;
        //auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
        //cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);
        //auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
        //srvHandle.Offset(srvIndex, mCbvSrvUavDescriptorSize);

        //CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvHeap->GetGPUDescriptorHandleForHeapStart()); //10.31
       // tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);//偏移到对应Mat的纹理上 //10.31


        //书上是写在Frame中的，但是在Frame中不是普遍情况，所以在此我写在Ritem中，来保证每个渲染物体组能有一个自己的缓冲区，就是不知道能不能成功
        auto instanceBuffer = ri->InstanceBuffer->Resource();
        cmdList->SetGraphicsRootShaderResourceView(4, instanceBuffer->GetGPUVirtualAddress());
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;//11.5
        //D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize; //10.31

        //cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle); 
        //cmdList->SetGraphicsRootDescriptorTable(2, srvHandle);

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);//物体的CB（0） PassCB（1） 11.5
        //cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);//材质CB（2 //10.31
        //cmdList->SetGraphicsRootDescriptorTable(3, tex);//SRV 放在(3) //10.31

        //绘制部分 （顶点数，顶点坐标的偏移位置，顶点的开始位置）
        //可能会疑惑，欸既然所有实例都在一个缓冲区里，那为什么你只需要visiableCount个数的实例就能进去呢。因为绘制的时候只绘制物体啊，你剔除的时候不是更改了缓冲区里的个数这些吗，那你矩阵不一样，你每个实例位置不就是不一样的吗，只是长得像，不过在创建物体时应该是传入了World的吧。
        cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);


    }
}
    array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
    {
        //6种静态采样器

        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
            0, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
            1, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
            2, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
            3, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
            4, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
            0.0f,                             // mipLODBias
            8);                               // maxAnisotropy

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
            5, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
            0.0f,                              // mipLODBias
            8);                                // maxAnisotropy

        return {
            pointWrap, pointClamp,
            linearWrap, linearClamp,
            anisotropicWrap, anisotropicClamp };
}
    /// <summary>
    ///  计算的是观察空间中的，sx sy 是屏幕空间坐标
    /// </summary>
    /// <param name="sx"></param>
    /// <param name="sy"></param>
    void ShapesApp::Pick(int sx, int sy)
    {
        XMFLOAT4X4 P = mCamera.GetProj4x4f();

        //计算观察空间中的拾取射线
        float vx = (+2.0f * sx / mClientWidth - 1.0f) / P(0, 0);
        float vy = (-2.0f * sy / mClientHeight + 1.0f) / P(1, 1);

        //位于观察空间里拾取射线的定义
        XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

        XMMATRIX V = mCamera.GetView();
        XMVECTOR dec1 = XMMatrixDeterminant(V);
        XMMATRIX invView = XMMatrixInverse(&dec1, V);

        mPickedRitem->Visible = false;

        for (auto ri : mRitemLayer[(int)RenderLayer::Opaque])
        {
            auto geo = ri->Geo;

            if (ri->Visible == false)
                continue;

            XMMATRIX W = XMLoadFloat4x4(&ri->World);
            XMVECTOR dec2 = XMMatrixDeterminant(W);
            XMMATRIX invWorld = XMMatrixInverse(&dec2, W);

            XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

            //转到局部空间    
            rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
            rayDir = XMVector3TransformNormal(rayDir, toLocal);

            rayDir = XMVector3Normalize(rayDir);

            //先判断是否碰到包围盒，再去判断三角形
            float tmin = 0.0f;
            if (ri->Bounds.Intersects(rayOrigin, rayDir, tmin))
            {
                //对不同格式混合在一起的化，我们需要用元数据(metadata)来进行强制转换
                auto vertices = (Vertex*)geo->VertexBufferCPU->GetBufferPointer();
                auto indices = (std::uint32_t*)geo->IndexBufferCPU->GetBufferPointer();
                UINT triCount = ri->IndexCount / 3;

                //对找到的里摄像机最近的三角形执行香蕉检测 banana
                tmin = MathHelper::Infinity;
                for (UINT i = 0; i < triCount; ++i)
                {
                    //三角形的索引
                    UINT i0 = indices[i * 3 + 0];
                    UINT i1 = indices[i * 3 + 1];
                    UINT i2 = indices[i * 3 + 2];

                   //构成三角形的顶点
                    XMVECTOR v0 = XMLoadFloat3(&vertices[i0].position);
                    XMVECTOR v1 = XMLoadFloat3(&vertices[i1].position);
                    XMVECTOR v2 = XMLoadFloat3(&vertices[i2].position);

                    //遍历网格上1所有三角形来找到最近的与拾取射线香蕉的三角形
                    float t = 0.0f;
                    if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
                    {
                        if (t < tmin)
                        {
                            //这就是更近的三角形
                            tmin = t;
                            UINT pickedTriangle = t;

                            //为北市区的三角形设置渲染项
                            mPickedRitem->Visible = true;
                            mPickedRitem->IndexCount = 3;
                            mPickedRitem->BaseVertexLocation = 0;

                            //拾取渲染项与拾取物体的联系传递
                            mPickedRitem->World = ri->World;

                            mPickedRitem->NumFramesDirty = gNumFrameResources;

                            //偏移到索引处
                            mPickedRitem->StartIndexLocation = 3 * pickedTriangle;
                        }
                    }
                }
            }
        }
    }