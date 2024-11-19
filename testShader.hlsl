#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif


#include "C:\Users\qaze6\source\repos\DirectTest\LightUtils.hlsl"

struct MaterialData
{
	float4   DiffuseAlbedo;
	float3   FresnelR0;
	float    Roughness;
	float4x4 MatTransform;
	uint     DiffuseMapIndex;
	uint     MatPad0;
	uint     MatPad1;
	uint     MatPad2;
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

//索引不对？
Texture2D gDiffuseMap[4] : register(t0);
/*
如果你在 HLSL 中将 MaterialData 按照 SRV 的布局来读取（假设 MaterialData 仅占用 108 字节），但实际上数据被当作 CBV 读取，HLSL 会把整个 256 字节块都当作 MaterialData 处理，这可能导致：

未定义的数据：因为填充值在 HLSL 中被读取了，但它并不包含有效的数据。
读取错误位置：如果试图读取超过 108 字节的填充区域，得到的可能是零或垃圾值。
*/
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t1,space1);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer obPass : register(b0)
{
     float4x4 gWorld;
	float4x4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
}
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
    Light Lights[MaxLights];
}


 struct VSInput
 {
     float4 position : POSITION;
     float3 normal : NORMAL;
     float2 texCoord : TEXCOORD;         
 };
struct PSInput
{
    nointerpolation uint MatIndex : MATINDEX;
    
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 worldPosition : POSITION;
    float3 normal : NORMAL;
};

PSInput VSMain(VSInput input,uint instanceID : SV_InstanceID)
{
   PSInput result;

   MaterialData matData = gMaterialData[gMaterialIndex];
    
    InstanceData instanceData = gInstanceData[instanceID];
    //result.worldPosition = mul(position, Model);
    //result.position = mul(position, MVP);
    //result.texCoord = texCoord;
    //result.normal = normal; //如果物体进行了非等比缩放，这里需要对法线进行模型矩阵左上角的逆矩阵的转置矩阵变换。

    result.MatIndex = instanceData.MaterialIndex;
    
    result.worldPosition = mul(input.position, instanceData.World);
    result.position = mul(result.worldPosition, VP);
   
    float4 texC = mul(float4(input.texCoord, 0.0f, 1.0f), gTexTransform);
    result.texCoord = mul(texC, matData.MatTransform).xy;
    result.normal = mul(input.normal, (float3x3)gWorld);
    //result.normal = normal;

    return result;


}

float4 PSMain(PSInput input) : SV_TARGET
{  

    //这里要更改 11.3
  MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamLinearWrap, input.texCoord);
  //float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, input.texCoord) * gDiffuseAlbedo;

  #ifdef ALPHA_TEST
	/*
    透明度测试：

    代码检查纹理的 alpha 值（透明度）是否小于某个阈值（这里是 0.1）。如果是，则该像素将被丢弃（即不渲染）。
    这通常通过使用 discard 语句实现。
    早期退出：

    由于该测试是在着色器的最早阶段执行，因此可以避免后续不必要的计算。如果像素被丢弃，那么后面的渲染和计算过程就会被跳过，从而节省 GPU 资源。
    优点
    */
    
	clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    input.normal = normalize(input.normal);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(CameraPosition - input.worldPosition.xyz);

    // Light terms.
    float4 ambient = AmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(Lights, mat, input.worldPosition.xyz,
        input.normal, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    float temp = (float) gMaterialIndex / 4.0f;
    return litColor;

}

