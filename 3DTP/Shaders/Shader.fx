cbuffer MonCB : register(b0)
{
  matrix WorldViewProj;
}

texture2D DiffuseMap : register(t0);
texture2D DetailMap : register(t1);

SamplerState LinearTextureSampler : register(s0)
{
  Filter = MIN_MAG_MIP_LINEAR;
  AddressU = Wrap;
  AddressV = Wrap;
};

struct VertexInput
{
  float3 position : POSITION;
  float2 uv : TEXCOORD0;
};

struct VertexOutput
{
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD0;
};

VertexOutput DiffuseVS(VertexInput input)
{
  VertexOutput output;

  output.position = mul(float4(input.position, 1.0f), WorldViewProj);
  output.uv = input.uv;

  return output;
}

float4 DiffusePS(VertexOutput input) :SV_Target
{
  float4 diffuseColor = DiffuseMap.Sample(LinearTextureSampler, input.uv);
  float4 detailColor = DetailMap.Sample(LinearTextureSampler, input.uv * 4);

  float4 color = diffuseColor * detailColor;

  return color;
}

TextureCube SkyMap;

struct SKYMAP_VS_OUTPUT    //output structure for skymap vertex shader
{
    float4 Pos : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

SKYMAP_VS_OUTPUT SKYMAP_VS(float3 inPos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL)
{
    SKYMAP_VS_OUTPUT output = (SKYMAP_VS_OUTPUT)0;

    //Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
    output.Pos = mul(float4(inPos, 1.0f), WorldViewProj).xyww;

    output.texCoord = inPos;

    return output;
}

float4 SKYMAP_PS(SKYMAP_VS_OUTPUT input) : SV_Target
{
    return SkyMap.Sample(LinearTextureSampler, input.texCoord);
}
