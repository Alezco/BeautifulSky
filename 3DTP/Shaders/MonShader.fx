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
