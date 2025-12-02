// DoF Phase 2 - Pass 1: Downsample + CoC 계산
// 2x2 박스 필터로 Half-res 다운샘플 + MRT로 Color와 CoC 출력

#include "DoF_Common.hlsli"

// 입력 텍스처 (Full-res)
Texture2D g_SceneColorTex : register(t0);
Texture2D g_DepthTex : register(t1);

// 샘플러
SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

// MRT 출력 구조체
struct PS_OUTPUT
{
    float4 Color : SV_Target0;  // Half-res Scene Color
    float CoC : SV_Target1;     // Half-res CoC (R16F)
};

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;

    // Half-res 렌더 타겟의 UV (0~1 전체 범위)
    float2 halfResUV = input.texCoord;

    // Full-res 소스 텍스처의 ViewRect 영역으로 UV 변환
    // Half-res 텍스처는 전체를 사용하지만, Full-res는 ViewRect 영역만 유효
    float2 sourceUV = RemapToSourceUV(halfResUV);

    // 2x2 박스 필터 오프셋 (Full-res 텍셀 단위 기준으로 계산)
    // 소스 텍스처 기준 텍셀 오프셋
    float2 srcTexelSize = float2(TexelSizeX, TexelSizeY) * float2(SourceUVScaleX, SourceUVScaleY);
    float2 offsets[4] = {
        float2(-0.5f, -0.5f) * srcTexelSize,
        float2( 0.5f, -0.5f) * srcTexelSize,
        float2(-0.5f,  0.5f) * srcTexelSize,
        float2( 0.5f,  0.5f) * srcTexelSize
    };

    float4 colorSum = float4(0, 0, 0, 0);
    float cocSum = 0;
    float cocMin = 999.0f;  // 전경 CoC 추적 (haloing 방지)

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 sampleUV = sourceUV + offsets[i];

        // 색상 샘플링 (Full-res 소스 텍스처)
        colorSum += g_SceneColorTex.Sample(g_LinearClampSampler, sampleUV);

        // 깊이 -> CoC (Full-res 깊이 텍스처)
        float depth = g_DepthTex.Sample(g_PointClampSampler, sampleUV).r;

        // 스카이박스 체크
        float linearDepth;
        float coc;
        if (depth >= 0.9999f)
        {
            // 스카이박스: 최대 배경 블러
            coc = MaxBlurRadius * 0.5f;  // Half-res 스케일
        }
        else
        {
            linearDepth = LinearizeDepth(depth);
            coc = CalculateCoCHalfRes(linearDepth);
        }

        cocSum += coc;
        cocMin = min(cocMin, coc);  // 가장 가까운 전경
    }

    output.Color = colorSum * 0.25f;

    // 전경 CoC 확장: 전경이 있으면 전경 CoC 사용 (haloing 방지)
    float avgCoc = cocSum * 0.25f;
    output.CoC = (cocMin < -0.5f) ? cocMin : avgCoc;

    return output;
}
