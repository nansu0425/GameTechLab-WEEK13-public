// DoF Phase 2 - Pass 3: Vertical Blur
// 수직 방향 9-tap 가우시안 블러 (CoC 가중)

#include "DoF_Common.hlsli"

// 입력 텍스처 (Half-res)
Texture2D g_SceneColorTex : register(t0);  // Half-res Source (Horizontal blur 결과)
Texture2D g_CoCTex : register(t1);         // Half-res CoC

// 샘플러
SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;
    float centerCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uv).r);

    // CoC가 작으면 블러 스킵 (초점 영역)
    if (centerCoc < 0.5f)
        return g_SceneColorTex.Sample(g_LinearClampSampler, uv);

    // 9-tap 가우시안 가중치 (중심 + 양방향 4개)
    static const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    // 수직 블러 방향
    float2 blurDir = float2(0, HalfTexelSizeY);

    // 블러 스케일 (CoC 기반)
    float blurScale = saturate(centerCoc / (MaxBlurRadius * 0.5f));

    // 중심 샘플
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSampler, uv);
    color += centerColor * weights[0];
    totalWeight += weights[0];

    // 양방향 샘플링
    [unroll]
    for (int i = 1; i < 5; i++)
    {
        float2 offset = blurDir * (float)i * blurScale * 2.0f;

        // 양의 방향
        float2 uvPos = uv + offset;
        float sampleCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uvPos).r);
        // CoC 가중: 작은 CoC를 가진 샘플은 가중치 감소 (haloing 방지)
        float weight = weights[i] * saturate(sampleCoc / max(centerCoc, 0.001f));
        color += g_SceneColorTex.Sample(g_LinearClampSampler, uvPos) * weight;
        totalWeight += weight;

        // 음의 방향
        float2 uvNeg = uv - offset;
        sampleCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uvNeg).r);
        weight = weights[i] * saturate(sampleCoc / max(centerCoc, 0.001f));
        color += g_SceneColorTex.Sample(g_LinearClampSampler, uvNeg) * weight;
        totalWeight += weight;
    }

    return color / max(totalWeight, 0.001f);
}
