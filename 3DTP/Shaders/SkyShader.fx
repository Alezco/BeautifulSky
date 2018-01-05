struct Light
{
    float3 dir;
    float4 ambient;
    float4 diffuse;
};

cbuffer cbPerFrame
{
    Light light;
};

cbuffer cbPerObject
{
    float4x4 WVP;
    float4x4 World;
};

Texture2D ObjTexture;
SamplerState ObjSamplerState;
TextureCube SkyMap;

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float3 normal : NORMAL;
};

struct SKYMAP_VS_OUTPUT    //output structure for skymap vertex shader
{
    float4 Pos : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

SKYMAP_VS_OUTPUT SKYMAP_VS(float3 inPos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL)
{
    SKYMAP_VS_OUTPUT output = (SKYMAP_VS_OUTPUT)0;

    //Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
    output.Pos = mul(float4(inPos, 1.0f), WVP).xyww;

    output.texCoord = inPos;

    return output;
}

float4 SKYMAP_PS(SKYMAP_VS_OUTPUT input) : SV_Target
{
    return SkyMap.Sample(ObjSamplerState, input.texCoord);
}
