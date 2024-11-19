#include "C:\Users\qaze6\source\repos\DirectTest\LightUtils.hlsl"



cbuffer cbPass : register(b1)
{
//推导一下V矩阵和P矩阵呢
    float4x4 M;
    float4x4 V;
    float4x4 P;
    float4x4 MVP;
    float4x4 VP;
    
    float4x4 PixelSpaceMat;
    float4x4 InvPixelSpaceMat;
    float4x4 N2S;
    
    float4 agentCameraPos;
    
    float PixelSize;
    float3 Padding3;
    
    //右向量用右手定则就行了
}
struct VSInput
{
   float4 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 Position : SV_Position;
    float2 uv : TEXCOORD;
    float4 worldPosition : POSITION;
    float3 Normal : NORMAL;
};
//需要在片元着色器里用，同时需要重建世界坐标
float2 WorldSpacePixelized(float4 worldPos)
{
    float4 PixelSpacePos = mul(worldPos, PixelSpaceMat);
    
    float PX = floor(PixelSpacePos.x / PixelSize);
    float PY = floor(PixelSpacePos.y / PixelSize);
    float PZ = floor(PixelSpacePos.z / PixelSize);
    
    float4 PixelizedPos = float4(PX, PY, PZ, PixelSpacePos.w);
    float4 PixelizedWorldPos = mul(PixelizedPos, InvPixelSpaceMat);
    float4 PixelizedClipPos = mul(PixelizedWorldPos, VP);
    PixelizedClipPos /= PixelizedClipPos.w;
    float2 PixelizedScreenPos = mul(PixelizedClipPos, N2S);
    
    return PixelizedScreenPos;
}
