// Depth of Field - 단일 패스 간소화 버전 (Phase 1)
// CoC 계산 + 디스크 블러를 하나의 패스로 처리

Texture2D g_DepthTex : register(t0);
Texture2D g_SceneColorTex : register(t1);

SamplerState g_PointClampSample : register(s0);
SamplerState g_LinearClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer DoFCB : register(b2)
{
    float FocalDistance;      // 초점 거리 (world units, cm)
    float CocScale;           // CoC 스케일 팩터 (무한대 CoC)
    float MaxBlurRadius;      // 최대 블러 반경 (픽셀 단위)
    float NearClip;           // 근거리 클리핑

    float FarClip;            // 원거리 클리핑
    float TexelSizeX;         // 1.0f / Width
    float TexelSizeY;         // 1.0f / Height
    float Padding;
}

// 깊이 버퍼 값 → 선형 깊이 (뷰 스페이스) 변환
// Mundi 엔진은 Z-Up, Left-Handed 좌표계 사용
float LinearizeDepth(float depth)
{
    // Reverse-Z를 사용하지 않는 경우의 표준 선형화
    // zView = Near * Far / (Far - depth * (Far - Near))
    return NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
}

// Circle of Confusion (CoC) 계산
// UE 방식: CoC = ((depth - focusDistance) / depth) × CocInfinity
// 음수 CoC: 전경 (카메라보다 가까움)
// 양수 CoC: 배경 (카메라보다 멀음)
float CalculateCoC(float linearDepth)
{
    // 초점 거리에서 CoC = 0
    // 무한대에서 CoC = CocScale (무한대 CoC)
    float coc = ((linearDepth - FocalDistance) / max(linearDepth, 0.001f)) * CocScale;

    // 최대 블러 반경으로 클램핑 (전경/배경 모두)
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// 간단한 디스크 블러 (Phase 1용)
// 8방향 + 중심 샘플링으로 가우시안 근사
float4 DiskBlur(float2 uv, float cocRadius)
{
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    // 8방향 오프셋 (원형 분포)
    static const int SAMPLE_COUNT = 8;
    static const float2 offsets[SAMPLE_COUNT] = {
        float2(1.0, 0.0),
        float2(0.707, 0.707),
        float2(0.0, 1.0),
        float2(-0.707, 0.707),
        float2(-1.0, 0.0),
        float2(-0.707, -0.707),
        float2(0.0, -1.0),
        float2(0.707, -0.707)
    };

    float blurRadius = abs(cocRadius);
    float2 texelSize = float2(TexelSizeX, TexelSizeY);

    // 중심 샘플 (가중치 높음)
    color += g_SceneColorTex.Sample(g_LinearClampSample, uv) * 2.0;
    totalWeight += 2.0;

    // 주변 샘플 (원형 분포)
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 offset = offsets[i] * blurRadius * texelSize;
        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, uv + offset);
        color += sampleColor;
        totalWeight += 1.0;
    }

    return color / totalWeight;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // 1. 깊이 샘플링 및 선형화
    float depth = g_DepthTex.Sample(g_PointClampSample, uv).r;

    // 스카이박스 체크 (depth가 1.0에 매우 가까운 경우)
    if (depth >= 0.9999f)
    {
        // 스카이박스는 항상 최대 블러 적용 (배경이므로)
        return DiskBlur(uv, MaxBlurRadius);
    }

    float linearDepth = LinearizeDepth(depth);

    // 2. CoC 계산
    float coc = CalculateCoC(linearDepth);

    // 3. CoC 크기에 따른 블러 적용
    // CoC가 매우 작으면 (초점 영역) 원본 반환
    if (abs(coc) < 0.5f)  // 0.5 픽셀 미만이면 블러 불필요
    {
        return g_SceneColorTex.Sample(g_LinearClampSample, uv);
    }
    else
    {
        // 비초점 영역: 디스크 블러 적용
        return DiskBlur(uv, coc);
    }
}
