struct VS_Input
{
    // light position
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct PS_Input
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

cbuffer LightViewProjBuffer : register(b0)
{
    row_major float4x4 LightViewProj;
}

Texture2D ShadowMask : register(t0);
SamplerState DefaultSampler : register(s0);

PS_Input mainVS(VS_Input Input)
{
    PS_Input Output;

    Output.Position = mul(float4(Input.Position, 1.0f), LightViewProj);
    Output.TexCoord = Input.TexCoord;

    return Output;
}

void mainPS(PS_Input Input)
{
    
    float4 Shadow = ShadowMask.Sample(DefaultSampler, Input.TexCoord);

    // 투명 픽셀은 기록하지 않는다.
    clip(Shadow.a - 0.15f);
}