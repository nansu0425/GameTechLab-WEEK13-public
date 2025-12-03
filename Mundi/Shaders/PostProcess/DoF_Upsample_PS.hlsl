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
    // 뷰포트가 ViewRect로 설정되어 있으므로 UV 0~1이 ViewRect 영역에 대응
    // 이 UV를 그대로 Half-res 블러 텍스처에 사용 (1:1 매핑)
    float2 localUV = input.texCoord;

    // Full-res 텍스처 샘플링을 위한 UV (전체 렌더타겟 기준)
    float2 fullResUV = RemapToSourceUV(localUV);

    // Half-res 블러 텍스처 UV (ViewRect 기반 Half-res 버퍼와 1:1 매핑)
    float2 halfResUV = localUV;

    // Full-res 깊이로 CoC 재계산
    float depth = g_DepthTex.Sample(g_PointClampSampler, fullResUV).r;

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

    // 바이리니어 업샘플 (Half-res 블러) - 리매핑된 UV 사용
    float4 blurColor = g_BlurTex.Sample(g_LinearClampSampler, halfResUV);

    // Full-res 원본 (선명) - Full-res UV 그대로 사용
    float4 sharpColor = g_SharpTex.Sample(g_LinearClampSampler, fullResUV);

    // CoC 기반 선명/블러 보간
    // coc < 0.5: 선명, coc >= MaxBlurRadius * 0.5: 블러
    float blendFactor = saturate((coc - 0.5f) / (MaxBlurRadius * 0.5f));

    return lerp(sharpColor, blurColor, blendFactor);
}
