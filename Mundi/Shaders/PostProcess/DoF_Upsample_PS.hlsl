// DoF Phase 2 - Pass 4: Upsample + Composite
// Half-res 블러 결과를 Full-res로 업샘플하고 원본과 블렌딩

#include "DoF_Common.hlsli"

// 입력 텍스처
Texture2D g_SharpTex : register(t0);   // Full-res 원본 (SceneColorSource)
Texture2D g_BlurTex : register(t1);    // Half-res 블러 결과
Texture2D g_DepthTex : register(t2);   // Full-res Depth

// 샘플러
SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // Full-res 깊이로 CoC 재계산
    float depth = g_DepthTex.Sample(g_PointClampSampler, uv).r;

    float coc;
    if (depth >= 0.9999f)
    {
        // 스카이박스: 최대 배경 블러
        coc = MaxBlurRadius;
    }
    else
    {
        float linearDepth = LinearizeDepth(depth);
        coc = abs(CalculateCoCFullRes(linearDepth));
    }

    // 바이리니어 업샘플 (Half-res 블러)
    float4 blurColor = g_BlurTex.Sample(g_LinearClampSampler, uv);

    // Full-res 원본 (선명)
    float4 sharpColor = g_SharpTex.Sample(g_LinearClampSampler, uv);

    // CoC 기반 선명/블러 보간
    // coc < 0.5: 선명, coc >= MaxBlurRadius * 0.5: 블러
    float blendFactor = saturate((coc - 0.5f) / (MaxBlurRadius * 0.5f));

    return lerp(sharpColor, blurColor, blendFactor);
}
