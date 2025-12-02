// DoF Phase 2 - 공통 상수 버퍼 및 유틸리티 함수

#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

// DoF 상수 버퍼 (b2) - 64 bytes
cbuffer DoFCB : register(b2)
{
    // Row 1 (16 bytes)
    float FocalDistance;      // 초점 거리 (world units)
    float CocScale;           // CoC 스케일 팩터 (정규화됨)
    float MaxBlurRadius;      // 최대 블러 반경 (픽셀 단위)
    float NearClip;           // 근거리 클리핑

    // Row 2 (16 bytes)
    float FarClip;            // 원거리 클리핑
    float TexelSizeX;         // 1.0f / FullWidth
    float TexelSizeY;         // 1.0f / FullHeight
    float HalfTexelSizeX;     // 1.0f / HalfWidth

    // Row 3 (16 bytes)
    float HalfTexelSizeY;     // 1.0f / HalfHeight
    float BlurDirection;      // 0.0f = Horizontal, 1.0f = Vertical
    float Padding1;
    float Padding2;

    // Row 4 (16 bytes)
    float Padding3;
    float Padding4;
    float Padding5;
    float Padding6;
}

// 깊이 버퍼 값 -> 선형 깊이 (뷰 스페이스) 변환
// Mundi 엔진: Z-Up, Left-Handed 좌표계
float LinearizeDepth(float depth)
{
    return NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
}

// CoC 계산 (픽셀 단위) - Full-res 기준
float CalculateCoCFullRes(float linearDepth)
{
    float ViewportHeight = 1.0f / TexelSizeY;
    float CocScalePixels = CocScale * ViewportHeight;
    float coc = ((linearDepth - FocalDistance) / max(linearDepth, 0.001f)) * CocScalePixels;
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// CoC 계산 (픽셀 단위) - Half-res 기준
float CalculateCoCHalfRes(float linearDepth)
{
    float HalfViewportHeight = 1.0f / HalfTexelSizeY;
    float CocScalePixels = CocScale * HalfViewportHeight;
    float coc = ((linearDepth - FocalDistance) / max(linearDepth, 0.001f)) * CocScalePixels;
    return clamp(coc, -MaxBlurRadius * 0.5f, MaxBlurRadius * 0.5f);
}

// 입력 구조체 (Full-screen triangle)
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

#endif // DOF_COMMON_HLSLI
