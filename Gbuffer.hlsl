
//这里弄成InstanceData 结构体
cbuffer obPass : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
}
//不变
cbuffer cbPass : register(b1)
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

struct VSIn
{
    float4 Pos : POSITIONT;
    float4 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct VSOut
{
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

VSOut VSGbuffer(VSIn varying)
{
    VSOut Output;
    Output.WPos = mul(gWorld, varying.Pos);
    Output.CPos = mul(VP, Output.WPos);
    Output.Normal = mul(gWorld, varying.Normal);
    Output.UV = mul(gTexTransform, varying.UV);
    
    return Output;
}
//下面这个没做完
PSOut PSGbuffer(VSOut fragment)
{
    PSOut Output;
    
    Output.Color = fragment;
    
    Output.Normal = fragment.Normal;
    
    Output.Position = fragment.WPos;
    
    Output.Depth = fragment.CPos.z;
    
    return Output;
}