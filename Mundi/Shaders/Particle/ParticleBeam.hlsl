// Global constants
cbuffer PerFrameConstants : register(b0)
{
    float4x4 ViewProjectionMatrix;
    float3 ViewOrigin;
    float Time;
};

cbuffer PerObjectConstants : register(b1)
{
    float4x4 WorldMatrix;
    float4 ObjectID;
    float CustomTime;
};

// Vertex Shader
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    // The incoming position is already in world space
    output.Position = mul(float4(input.Position, 1.0f), ViewProjectionMatrix);
    output.UV = input.UV;
    output.Color = input.Color;
    return output;
}

// Pixel Shader
Texture2D DiffuseTexture : register(t0);
SamplerState DefaultSampler : register(s0);

float4 PS_Main(PS_INPUT input) : SV_Target
{
    // For now, just use vertex color multiplied by a simple texture lookup.
    // If texture is not bound, it might return black. For initial test, just returning color might be safer.
    // return input.Color;
    float4 Albedo = DiffuseTexture.Sample(DefaultSampler, input.UV);
    return input.Color * Albedo;
}