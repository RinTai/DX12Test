
//不变
cbuffer cbPass : register(b0)
{
//推导一下V矩阵和P矩阵呢
    float4x4 M;
    float4x4 V;
    float4x4 P;
    float4x4 MVP;
    float4x4 VP;
    
    float3 CameraPosition;
    float cbPerObjectPad1;

    float gTotalTime;
    float gDeltaTime;
    float2 cbPerObjectPad2;

    float4 AmbientLight;
    float2 cbPerObjectPad3;
}
/*

这里加MatCB（弄成Struct） 


*/

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint MatPad0;
    uint MatPad1;
    uint MatPad2;
};

struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint InstPad0;
    uint InstPad1;
    uint InstPad2;
};

Texture2D gDiffuseMap[4] : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

/*struct GbufferConstant
{
    float4x4 World;
    float4x4 TexTransform;
    float MatIndex;
    uint3 Padding1;
};

StructuredBuffer<GbufferConstant> GbufferData : register(b0);*/
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t1, space1);

struct VSIn
{
    float4 Pos : POSITIONT;
    float4 Normal : NORMAL;
    float2 UV : TEXCOORD0;

};

struct VSOut
{
    nointerpolation uint MatIndex : MATINDEX;
    
    float4 CPos : SV_Position;
    float4 WPos : POSITION;
    float4 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct PSOut
{
    float4 Normal : SV_TARGET0;
    float4 Position : SV_TARGET1;
    float4 Color : SV_TARGET2;
    float Depth : SV_TARGET3;
};

VSOut VSGbuffer(VSIn varying, uint instanceID : SV_InstanceID)
{
    
    InstanceData instanceData = gInstanceData[instanceID];
    MaterialData matData = gMaterialData[instanceData.MaterialIndex];
    
    VSOut Output;
    Output.WPos = mul(instanceData.World, varying.Pos);
    Output.CPos = mul(VP, Output.WPos);
    Output.Normal = mul(instanceData.World, varying.Normal);
    float4 texC = mul(float4(varying.UV, 0.0f, 1.0f), instanceData.TexTransform);
    Output.UV = mul(texC, matData.MatTransform).xy;
    Output.MatIndex = instanceData.MaterialIndex;
    
    return Output;
}
//下面这个没做完
PSOut PSGbuffer(VSOut fragment)
{
    MaterialData matData = gMaterialData[fragment.MatIndex];
    
    PSOut Output;
    
    Output.Color = fragment;
    
    Output.Normal = fragment.Normal;
    
    Output.Position = fragment.WPos;
    
    Output.Depth = fragment.CPos.z;
    
    return Output;
}