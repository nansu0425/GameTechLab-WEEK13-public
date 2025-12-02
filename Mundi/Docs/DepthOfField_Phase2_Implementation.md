# DoF (Depth of Field) Phase 2 구현 계획

## 목표

Phase 1의 단일 패스 DoF를 다중 패스 구조로 개선하여 성능과 품질 향상

| 항목 | 현재 (Phase 1) | 목표 (Phase 2) | 개선 효과 |
|------|---------------|---------------|----------|
| 블러 알고리즘 | 8방향 DiskBlur | Separable Blur | 샘플 수 N² → 2N |
| 해상도 | Full-resolution | Half-resolution | 픽셀 수 75%↓ |
| 전경/배경 | 단일 처리 | 분리 처리 (선택) | Haloing 제거 |

---

## 구현 순서

```
Step 1: RHI 확장 (Half-res 렌더 타겟)
    ↓
Step 2: 상수 버퍼 확장 (FDoFBufferType)
    ↓
Step 3: 셰이더 생성 (4개 파일)
    ↓
Step 4: DepthOfFieldPass 다중 패스 구현
    ↓
Step 5: 테스트 및 검증
```

---

## 렌더 패스 파이프라인

```
[입력] SceneColor (Full-res) + SceneDepth (Full-res)
                    │
                    ▼
┌──────────────────────────────────────────────────────────────┐
│ Pass 1: Downsample + CoC 계산                                │
│ 셰이더: DoF_Downsample_PS.hlsl                               │
│ 입력: t0=SceneColor, t1=SceneDepth                           │
│ 출력: RTV0=HalfRes_Color, RTV1=HalfRes_CoC (MRT)             │
│ 뷰포트: Half-resolution                                      │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────┐
│ Pass 2: Horizontal Blur                                      │
│ 셰이더: DoF_HorizontalBlur_PS.hlsl                           │
│ 입력: t0=HalfRes_Color(Source), t1=HalfRes_CoC               │
│ 출력: HalfRes_Color(Target) - Ping-Pong                      │
│ 상수: BlurDirection = 0                                      │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────┐
│ Pass 3: Vertical Blur                                        │
│ 셰이더: DoF_VerticalBlur_PS.hlsl                             │
│ 입력: t0=HalfRes_Color(Source), t1=HalfRes_CoC               │
│ 출력: HalfRes_Color(Target) - Ping-Pong                      │
│ 상수: BlurDirection = 1                                      │
└──────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌──────────────────────────────────────────────────────────────┐
│ Pass 4: Upsample + Composite                                 │
│ 셰이더: DoF_Upsample_PS.hlsl                                 │
│ 입력: t0=FullRes_Original, t1=HalfRes_Blurred, t2=Depth      │
│ 출력: SceneColor (Final) - FSwapGuard 사용                   │
│ 뷰포트: Full-resolution                                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Step 1: RHI 확장

### 파일: `Source/Runtime/RHI/D3D11RHI.h`

**추가할 멤버 변수:**

```cpp
private:
    // ===== DoF Phase 2: Half-resolution 버퍼 =====
    static const int NUM_HALF_RES_BUFFERS = 2;

    // Half-res Scene Color (Ping-Pong)
    ID3D11Texture2D* HalfResColorTextures[NUM_HALF_RES_BUFFERS] = { nullptr, nullptr };
    ID3D11RenderTargetView* HalfResColorRTVs[NUM_HALF_RES_BUFFERS] = { nullptr, nullptr };
    ID3D11ShaderResourceView* HalfResColorSRVs[NUM_HALF_RES_BUFFERS] = { nullptr, nullptr };

    // Half-res CoC (R16F, 단일 버퍼)
    ID3D11Texture2D* HalfResCoCTexture = nullptr;
    ID3D11RenderTargetView* HalfResCoCRTV = nullptr;
    ID3D11ShaderResourceView* HalfResCoCSRV = nullptr;

    // Ping-Pong 인덱스
    int32 HalfResSourceIndex = 0;
    int32 HalfResTargetIndex = 1;

    // Half-res 뷰포트 캐시
    D3D11_VIEWPORT HalfResViewport = {};

public:
    // ===== DoF Phase 2: Half-res 버퍼 관리 =====
    void CreateHalfResolutionBuffers(UINT FullWidth, UINT FullHeight);
    void ReleaseHalfResolutionBuffers();

    // Half-res Ping-Pong
    void SwapHalfResRenderTargets();
    ID3D11ShaderResourceView* GetHalfResColorSourceSRV() const;
    ID3D11RenderTargetView* GetHalfResColorTargetRTV() const;
    ID3D11ShaderResourceView* GetHalfResCoCSRV() const;
    ID3D11RenderTargetView* GetHalfResCoCRTV() const;

    // 뷰포트 전환
    void SetHalfResViewport();
    void RestoreFullResViewport();
```

### 파일: `Source/Runtime/RHI/D3D11RHI.cpp`

**추가할 함수 구현:**

```cpp
void D3D11RHI::CreateHalfResolutionBuffers(UINT FullWidth, UINT FullHeight)
{
    ReleaseHalfResolutionBuffers();  // 기존 버퍼 해제

    UINT HalfWidth = FullWidth / 2;
    UINT HalfHeight = FullHeight / 2;

    // Half-res 뷰포트 캐시
    HalfResViewport.Width = (float)HalfWidth;
    HalfResViewport.Height = (float)HalfHeight;
    HalfResViewport.MinDepth = 0.0f;
    HalfResViewport.MaxDepth = 1.0f;
    HalfResViewport.TopLeftX = 0.0f;
    HalfResViewport.TopLeftY = 0.0f;

    // ===== Half-res Color 버퍼 (Ping-Pong) =====
    D3D11_TEXTURE2D_DESC ColorDesc = {};
    ColorDesc.Width = HalfWidth;
    ColorDesc.Height = HalfHeight;
    ColorDesc.MipLevels = 1;
    ColorDesc.ArraySize = 1;
    ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorDesc.SampleDesc.Count = 1;
    ColorDesc.Usage = D3D11_USAGE_DEFAULT;
    ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    for (int i = 0; i < NUM_HALF_RES_BUFFERS; i++)
    {
        Device->CreateTexture2D(&ColorDesc, nullptr, &HalfResColorTextures[i]);
        Device->CreateRenderTargetView(HalfResColorTextures[i], nullptr, &HalfResColorRTVs[i]);
        Device->CreateShaderResourceView(HalfResColorTextures[i], nullptr, &HalfResColorSRVs[i]);
    }

    // ===== Half-res CoC 버퍼 (R16F) =====
    D3D11_TEXTURE2D_DESC CoCDesc = {};
    CoCDesc.Width = HalfWidth;
    CoCDesc.Height = HalfHeight;
    CoCDesc.MipLevels = 1;
    CoCDesc.ArraySize = 1;
    CoCDesc.Format = DXGI_FORMAT_R16_FLOAT;
    CoCDesc.SampleDesc.Count = 1;
    CoCDesc.Usage = D3D11_USAGE_DEFAULT;
    CoCDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Device->CreateTexture2D(&CoCDesc, nullptr, &HalfResCoCTexture);
    Device->CreateRenderTargetView(HalfResCoCTexture, nullptr, &HalfResCoCRTV);
    Device->CreateShaderResourceView(HalfResCoCTexture, nullptr, &HalfResCoCSRV);

    HalfResSourceIndex = 0;
    HalfResTargetIndex = 1;
}

void D3D11RHI::ReleaseHalfResolutionBuffers()
{
    for (int i = 0; i < NUM_HALF_RES_BUFFERS; i++)
    {
        if (HalfResColorSRVs[i]) { HalfResColorSRVs[i]->Release(); HalfResColorSRVs[i] = nullptr; }
        if (HalfResColorRTVs[i]) { HalfResColorRTVs[i]->Release(); HalfResColorRTVs[i] = nullptr; }
        if (HalfResColorTextures[i]) { HalfResColorTextures[i]->Release(); HalfResColorTextures[i] = nullptr; }
    }
    if (HalfResCoCSRV) { HalfResCoCSRV->Release(); HalfResCoCSRV = nullptr; }
    if (HalfResCoCRTV) { HalfResCoCRTV->Release(); HalfResCoCRTV = nullptr; }
    if (HalfResCoCTexture) { HalfResCoCTexture->Release(); HalfResCoCTexture = nullptr; }
}

void D3D11RHI::SwapHalfResRenderTargets()
{
    std::swap(HalfResSourceIndex, HalfResTargetIndex);
}

ID3D11ShaderResourceView* D3D11RHI::GetHalfResColorSourceSRV() const
{
    return HalfResColorSRVs[HalfResSourceIndex];
}

ID3D11RenderTargetView* D3D11RHI::GetHalfResColorTargetRTV() const
{
    return HalfResColorRTVs[HalfResTargetIndex];
}

ID3D11ShaderResourceView* D3D11RHI::GetHalfResCoCSRV() const
{
    return HalfResCoCSRV;
}

ID3D11RenderTargetView* D3D11RHI::GetHalfResCoCRTV() const
{
    return HalfResCoCRTV;
}

void D3D11RHI::SetHalfResViewport()
{
    DeviceContext->RSSetViewports(1, &HalfResViewport);
}

void D3D11RHI::RestoreFullResViewport()
{
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}
```

**OnResize() 수정:**

```cpp
void D3D11RHI::OnResize(UINT NewWidth, UINT NewHeight)
{
    // ... 기존 코드 ...

    // Half-res 버퍼도 재생성
    CreateHalfResolutionBuffers(NewWidth, NewHeight);
}
```

---

## Step 2: 상수 버퍼 확장

### 파일: `Source/Runtime/RHI/ConstantBufferType.h`

**FDoFBufferType 수정 (32바이트 → 64바이트):**

```cpp
struct alignas(16) FDoFBufferType
{
    // ===== Row 1 (16 bytes) - 기존 =====
    float FocalDistance;      // 초점 거리 (world units)
    float CocScale;           // CoC 스케일 (정규화됨)
    float MaxBlurRadius;      // 최대 블러 반경 (픽셀)
    float NearClip;           // Near 클리핑 평면

    // ===== Row 2 (16 bytes) - 기존 + 확장 =====
    float FarClip;            // Far 클리핑 평면
    float TexelSizeX;         // 1.0f / FullWidth
    float TexelSizeY;         // 1.0f / FullHeight
    float HalfTexelSizeX;     // 1.0f / HalfWidth (NEW)

    // ===== Row 3 (16 bytes) - NEW =====
    float HalfTexelSizeY;     // 1.0f / HalfHeight
    float BlurDirection;      // 0.0f = Horizontal, 1.0f = Vertical
    float Padding1;           // 예약
    float Padding2;           // 예약

    // ===== Row 4 (16 bytes) - NEW =====
    float Padding3;           // 예약
    float Padding4;           // 예약
    float Padding5;           // 예약
    float Padding6;           // 예약
};
static_assert(sizeof(FDoFBufferType) == 64, "FDoFBufferType must be 64 bytes");

// 기존 CONSTANT_BUFFER_INFO 유지 (슬롯 b2)
```

---

## Step 3: 셰이더 생성

### 파일: `Shaders/PostProcess/DoF_Common.hlsli`

**공통 함수 및 상수 버퍼 (새로 생성):**

```hlsl
// DoF 공통 상수 버퍼 (b2)
cbuffer DoFCB : register(b2)
{
    // Row 1
    float FocalDistance;
    float CocScale;
    float MaxBlurRadius;
    float NearClip;

    // Row 2
    float FarClip;
    float TexelSizeX;
    float TexelSizeY;
    float HalfTexelSizeX;

    // Row 3
    float HalfTexelSizeY;
    float BlurDirection;
    float Padding1;
    float Padding2;

    // Row 4
    float Padding3;
    float Padding4;
    float Padding5;
    float Padding6;
}

// 깊이 선형화
float LinearizeDepth(float depth)
{
    return NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
}

// CoC 계산 (픽셀 단위)
float CalculateCoC(float linearDepth, float viewportHeight)
{
    float CocScalePixels = CocScale * viewportHeight;
    float coc = ((linearDepth - FocalDistance) / max(linearDepth, 0.001f)) * CocScalePixels;
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// Full-res CoC 계산
float CalculateCoCFullRes(float linearDepth)
{
    return CalculateCoC(linearDepth, 1.0f / TexelSizeY);
}

// Half-res CoC 계산
float CalculateCoCHalfRes(float linearDepth)
{
    return CalculateCoC(linearDepth, 1.0f / HalfTexelSizeY);
}
```

### 파일: `Shaders/PostProcess/DoF_Downsample_PS.hlsl`

```hlsl
#include "DoF_Common.hlsli"

// 입력 텍스처
Texture2D g_SceneColorTex : register(t0);
Texture2D g_DepthTex : register(t1);

// 샘플러
SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

// MRT 출력
struct PS_OUTPUT
{
    float4 Color : SV_Target0;  // Half-res Scene Color
    float CoC : SV_Target1;     // Half-res CoC (R16F)
};

// 입력 구조체
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;
    float2 uv = input.texCoord;

    // 2x2 박스 필터 오프셋 (Full-res 텍셀 단위)
    float2 offsets[4] = {
        float2(-0.5f, -0.5f) * float2(TexelSizeX, TexelSizeY),
        float2( 0.5f, -0.5f) * float2(TexelSizeX, TexelSizeY),
        float2(-0.5f,  0.5f) * float2(TexelSizeX, TexelSizeY),
        float2( 0.5f,  0.5f) * float2(TexelSizeX, TexelSizeY)
    };

    float4 colorSum = float4(0, 0, 0, 0);
    float cocSum = 0;
    float cocMin = 999.0f;  // 전경 CoC 추적 (haloing 방지)

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 sampleUV = uv + offsets[i];

        // 색상 샘플링
        colorSum += g_SceneColorTex.Sample(g_LinearClampSampler, sampleUV);

        // 깊이 → CoC
        float depth = g_DepthTex.Sample(g_PointClampSampler, sampleUV).r;
        float linearDepth = LinearizeDepth(depth);
        float coc = CalculateCoCFullRes(linearDepth);

        cocSum += coc;
        cocMin = min(cocMin, coc);  // 가장 가까운 전경
    }

    output.Color = colorSum * 0.25f;

    // 전경 CoC 확장: 전경이 있으면 전경 CoC 사용 (haloing 방지)
    float avgCoc = cocSum * 0.25f;
    output.CoC = (cocMin < -0.5f) ? cocMin : avgCoc;

    return output;
}
```

### 파일: `Shaders/PostProcess/DoF_HorizontalBlur_PS.hlsl`

```hlsl
#include "DoF_Common.hlsli"

Texture2D g_SceneColorTex : register(t0);  // Half-res Source
Texture2D g_CoCTex : register(t1);         // Half-res CoC

SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;
    float centerCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uv).r);

    // CoC가 작으면 블러 스킵
    if (centerCoc < 0.5f)
        return g_SceneColorTex.Sample(g_LinearClampSampler, uv);

    // 9-tap 가우시안 가중치
    static const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    // 수평 블러 방향
    float2 blurDir = float2(HalfTexelSizeX, 0);

    // 블러 스케일 (CoC 기반)
    float blurScale = centerCoc / MaxBlurRadius;

    // 중심 샘플
    color += g_SceneColorTex.Sample(g_LinearClampSampler, uv) * weights[0];
    totalWeight += weights[0];

    // 양방향 샘플링
    [unroll]
    for (int i = 1; i < 5; i++)
    {
        float2 offset = blurDir * (float)i * blurScale;

        // 양의 방향
        float2 uvPos = uv + offset;
        float sampleCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uvPos).r);
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
```

### 파일: `Shaders/PostProcess/DoF_VerticalBlur_PS.hlsl`

```hlsl
#include "DoF_Common.hlsli"

Texture2D g_SceneColorTex : register(t0);  // Half-res Source
Texture2D g_CoCTex : register(t1);         // Half-res CoC

SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;
    float centerCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uv).r);

    // CoC가 작으면 블러 스킵
    if (centerCoc < 0.5f)
        return g_SceneColorTex.Sample(g_LinearClampSampler, uv);

    // 9-tap 가우시안 가중치
    static const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    // 수직 블러 방향
    float2 blurDir = float2(0, HalfTexelSizeY);

    // 블러 스케일 (CoC 기반)
    float blurScale = centerCoc / MaxBlurRadius;

    // 중심 샘플
    color += g_SceneColorTex.Sample(g_LinearClampSampler, uv) * weights[0];
    totalWeight += weights[0];

    // 양방향 샘플링
    [unroll]
    for (int i = 1; i < 5; i++)
    {
        float2 offset = blurDir * (float)i * blurScale;

        // 양의 방향
        float2 uvPos = uv + offset;
        float sampleCoc = abs(g_CoCTex.Sample(g_PointClampSampler, uvPos).r);
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
```

### 파일: `Shaders/PostProcess/DoF_Upsample_PS.hlsl`

```hlsl
#include "DoF_Common.hlsli"

Texture2D g_SharpTex : register(t0);   // Full-res 원본 (SceneColorSource)
Texture2D g_BlurTex : register(t1);    // Half-res 블러 결과
Texture2D g_DepthTex : register(t2);   // Full-res Depth

SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // Full-res 깊이로 CoC 재계산
    float depth = g_DepthTex.Sample(g_PointClampSampler, uv).r;
    float linearDepth = LinearizeDepth(depth);
    float coc = abs(CalculateCoCFullRes(linearDepth));

    // 바이리니어 업샘플 (Half-res 블러)
    float4 blurColor = g_BlurTex.Sample(g_LinearClampSampler, uv);

    // Full-res 원본 (선명)
    float4 sharpColor = g_SharpTex.Sample(g_LinearClampSampler, uv);

    // CoC 기반 선명/블러 보간
    // coc < 0.5: 선명, coc >= MaxBlurRadius * 0.5: 블러
    float blendFactor = saturate((coc - 0.5f) / (MaxBlurRadius * 0.5f));

    return lerp(sharpColor, blurColor, blendFactor);
}
```

---

## Step 4: DepthOfFieldPass 수정

### 파일: `Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.h`

**수정된 클래스:**

```cpp
#pragma once
#include "PostProcessing.h"

class FDepthOfFieldPass final : public IPostProcessPass
{
public:
    FDepthOfFieldPass() = default;
    ~FDepthOfFieldPass() = default;

    void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;
    bool IsApplicable(const FPostProcessModifier& M) const override;

private:
    // 셰이더 캐시
    UShader* FullScreenVS = nullptr;
    UShader* DownsamplePS = nullptr;
    UShader* HorizontalBlurPS = nullptr;
    UShader* VerticalBlurPS = nullptr;
    UShader* UpsamplePS = nullptr;

    // 셰이더 로드
    void LoadShaders();

    // 상수 버퍼 업데이트
    void UpdateConstantBuffer(const FPostProcessModifier& M, FSceneView* View,
                              D3D11RHI* RHIDevice, float BlurDirection);

    // SRV 언바인드 헬퍼
    void ClearSRVs(D3D11RHI* RHIDevice, UINT StartSlot, UINT Count);
};
```

### 파일: `Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.cpp`

**Execute() 구현:**

```cpp
#include "pch.h"
#include "DepthOfFieldPass.h"
#include "SceneView.h"
#include "RHIDevice.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "SwapGuard.h"
#include "ConstantBufferType.h"

void FDepthOfFieldPass::LoadShaders()
{
    if (FullScreenVS) return;  // 이미 로드됨

    FullScreenVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    DownsamplePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Downsample_PS.hlsl");
    HorizontalBlurPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_HorizontalBlur_PS.hlsl");
    VerticalBlurPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_VerticalBlur_PS.hlsl");
    UpsamplePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Upsample_PS.hlsl");
}

void FDepthOfFieldPass::UpdateConstantBuffer(const FPostProcessModifier& M, FSceneView* View,
                                              D3D11RHI* RHIDevice, float BlurDirection)
{
    uint32 ViewWidth = View->ViewRect.Width();
    uint32 ViewHeight = View->ViewRect.Height();

    FDoFBufferType DoFCB;
    DoFCB.FocalDistance = M.Payload.Params0.X;
    DoFCB.CocScale = M.Payload.Params0.Y;
    DoFCB.MaxBlurRadius = M.Payload.Params0.Z;
    DoFCB.NearClip = View->NearClip;
    DoFCB.FarClip = View->FarClip;
    DoFCB.TexelSizeX = 1.0f / (float)ViewWidth;
    DoFCB.TexelSizeY = 1.0f / (float)ViewHeight;
    DoFCB.HalfTexelSizeX = 2.0f / (float)ViewWidth;
    DoFCB.HalfTexelSizeY = 2.0f / (float)ViewHeight;
    DoFCB.BlurDirection = BlurDirection;

    RHIDevice->SetAndUpdateConstantBuffer(DoFCB);
}

void FDepthOfFieldPass::ClearSRVs(D3D11RHI* RHIDevice, UINT StartSlot, UINT Count)
{
    ID3D11ShaderResourceView* NullSRVs[8] = { nullptr };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(StartSlot, Count, NullSRVs);
}

bool FDepthOfFieldPass::IsApplicable(const FPostProcessModifier& M) const
{
    return M.bEnabled && M.Type == EPostProcessEffectType::DepthOfField;
}

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M) || !View) return;

    LoadShaders();

    // 샘플러 바인딩 (전체 패스에서 공통)
    ID3D11SamplerState* Samplers[2] = {
        RHIDevice->GetSamplerState(RHI_Sampler_Index::Default),      // Linear
        RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp)    // Point
    };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

    // 렌더 스테이트 설정
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // ========== Pass 1: Downsample + CoC ==========
    {
        RHIDevice->SetHalfResViewport();

        // MRT: Color(RTV0) + CoC(RTV1)
        ID3D11RenderTargetView* RTVs[2] = {
            RHIDevice->GetHalfResColorTargetRTV(),
            RHIDevice->GetHalfResCoCRTV()
        };
        RHIDevice->GetDeviceContext()->OMSetRenderTargets(2, RTVs, nullptr);

        // SRV: SceneColor(t0) + Depth(t1)
        ID3D11ShaderResourceView* SRVs[2] = {
            RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource),
            RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth)
        };
        RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, SRVs);

        UpdateConstantBuffer(M, View, RHIDevice, 0.0f);
        RHIDevice->PrepareShader(FullScreenVS, DownsamplePS);
        RHIDevice->DrawFullScreenQuad();

        ClearSRVs(RHIDevice, 0, 2);
    }

    // ========== Pass 2: Horizontal Blur ==========
    {
        RHIDevice->SwapHalfResRenderTargets();

        ID3D11RenderTargetView* RTV = RHIDevice->GetHalfResColorTargetRTV();
        RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);

        ID3D11ShaderResourceView* SRVs[2] = {
            RHIDevice->GetHalfResColorSourceSRV(),
            RHIDevice->GetHalfResCoCSRV()
        };
        RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, SRVs);

        UpdateConstantBuffer(M, View, RHIDevice, 0.0f);  // Horizontal
        RHIDevice->PrepareShader(FullScreenVS, HorizontalBlurPS);
        RHIDevice->DrawFullScreenQuad();

        ClearSRVs(RHIDevice, 0, 2);
    }

    // ========== Pass 3: Vertical Blur ==========
    {
        RHIDevice->SwapHalfResRenderTargets();

        ID3D11RenderTargetView* RTV = RHIDevice->GetHalfResColorTargetRTV();
        RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);

        ID3D11ShaderResourceView* SRVs[2] = {
            RHIDevice->GetHalfResColorSourceSRV(),
            RHIDevice->GetHalfResCoCSRV()
        };
        RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, SRVs);

        UpdateConstantBuffer(M, View, RHIDevice, 1.0f);  // Vertical
        RHIDevice->PrepareShader(FullScreenVS, VerticalBlurPS);
        RHIDevice->DrawFullScreenQuad();

        ClearSRVs(RHIDevice, 0, 2);
    }

    // ========== Pass 4: Upsample + Composite ==========
    {
        RHIDevice->RestoreFullResViewport();

        // 기존 Ping-Pong 스왑 (3개 SRV 언바인드)
        FSwapGuard Swap(RHIDevice, 0, 3);
        RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

        // SRV: FullRes_Original(t0) + HalfRes_Blurred(t1) + FullRes_Depth(t2)
        ID3D11ShaderResourceView* SRVs[3] = {
            RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource),
            RHIDevice->GetHalfResColorSourceSRV(),
            RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth)
        };
        RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 3, SRVs);

        UpdateConstantBuffer(M, View, RHIDevice, 0.0f);
        RHIDevice->PrepareShader(FullScreenVS, UpsamplePS);
        RHIDevice->DrawFullScreenQuad();

        Swap.Commit();
    }
}
```

---

## Step 5: 테스트 체크리스트

### 빌드 확인
- [ ] 컴파일 에러 없음
- [ ] 링크 에러 없음

### 기능 테스트
- [ ] DoF 비활성화 시 기존과 동일하게 렌더링
- [ ] DoF 활성화 시 블러 효과 적용
- [ ] 초점 영역 선명함 유지
- [ ] FocalDistance 변경 시 초점 이동

### RenderDoc 검증
- [ ] Pass 1: Half-res Color + CoC 출력 확인
- [ ] Pass 2: 수평 블러 적용 확인
- [ ] Pass 3: 수직 블러 적용 확인
- [ ] Pass 4: Full-res 합성 확인

### 성능 확인
- [ ] GPU 프로파일러로 DoF 시간 측정
- [ ] Phase 1 대비 성능 개선 확인 (목표: 50%+)

---

## 파일 체크리스트

### 새로 생성
- [ ] `Shaders/PostProcess/DoF_Common.hlsli`
- [ ] `Shaders/PostProcess/DoF_Downsample_PS.hlsl`
- [ ] `Shaders/PostProcess/DoF_HorizontalBlur_PS.hlsl`
- [ ] `Shaders/PostProcess/DoF_VerticalBlur_PS.hlsl`
- [ ] `Shaders/PostProcess/DoF_Upsample_PS.hlsl`

### 수정
- [ ] `Source/Runtime/RHI/D3D11RHI.h` - Half-res 멤버 추가
- [ ] `Source/Runtime/RHI/D3D11RHI.cpp` - Half-res 함수 구현
- [ ] `Source/Runtime/RHI/ConstantBufferType.h` - FDoFBufferType 확장
- [ ] `Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.h` - 클래스 수정
- [ ] `Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.cpp` - 4패스 구현

---

## 예상 성능 비교

| 항목 | Phase 1 | Phase 2 |
|------|---------|---------|
| 해상도 | 1920×1080 | 960×540 |
| 픽셀 수 | 2,073,600 | 518,400 |
| 샘플/픽셀 | 9 | 9 (분리) |
| 총 샘플링 | 18.6M | 4.7M + 업샘플 |
| 예상 시간 | 1-2ms | 0.3-0.8ms |
