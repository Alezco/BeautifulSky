struct VertexInput
{
  float3 position : POSITION;
  float3 color : COLOR0;
};

struct VertexOutput
{
  float4 position : SV_POSITION;
  float3 color : COLOR0;
};

VertexOutput DiffuseVS(VertexInput input)
{
  VertexOutput output;

  output.position = float4(input.position, 1.0f);
  output.color = input.color;

  return output;
}

float4 DiffusePS(VertexOutput input) :SV_Target
{
  //return float4(1.0f, 1.0f, 0.0f, 1.0f);
  return float4(input.color, 1.0f);
}
