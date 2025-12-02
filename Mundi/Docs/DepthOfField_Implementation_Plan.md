# Depth of Field Phase 1 구현 계획

## 개요

Mundi 엔진에 Depth of Field 기능을 추가합니다.
분석 문서 [DepthOfField_Analysis.md](./DepthOfField_Analysis.md) 참조.

---

## 구현 순서

1. PostProcessing.h 수정 - DepthOfField 타입 추가
2. ConstantBufferType.h 수정 - FDoFBufferType 추가
3. DoF_Simple_PS.hlsl 생성 - 단일 패스 DoF 셰이더
4. DepthOfFieldPass 구현 - 포스트 프로세스 패스 클래스
5. SceneRenderer 통합 - DoF 패스 실행 추가
6. CamMod_DoF.h 생성 - 카메라 모디파이어 클래스
7. PlayerCameraManager 확장 - DoF API 추가
8. 테스트 및 디버깅

---

## Step 1: PostProcessing.h 수정

**파일**: `Mundi/Source/Runtime/Renderer/PostProcessing/PostProcessing.h`

**변경 내용**: EPostProcessEffectType에 DepthOfField 추가

```cpp
enum class EPostProcessEffectType : uint8
{
    HeightFog,
    Vignette,
    Bloom,
    Fade,
    Gamma,
    DepthOfField,  // 추가
};
```

---

## Step 2: ConstantBufferType.h 수정

**파일**: `Mundi/Source/Runtime/RHI/ConstantBufferType.h`

**변경 내용**: FDoFBufferType 구조체 추가

```cpp
// b2 - DoF 파라미터 (register b2, 기존 포스트프로세스 버퍼 슬롯)
struct FDoFBufferType
{
    float FocalDistance;      // 초점 거리 (world units, cm)
    float CocScale;           // CoC 스케일 팩터 (무한대 CoC)
    float MaxBlurRadius;      // 최대 블러 반경 (화면 비율)
    float NearClip;           // 근거리 클리핑

    float FarClip;            // 원거리 클리핑
    float TexelSizeX;         // 1/Width
    float TexelSizeY;         // 1/Height
    float Padding;
};
```

**CONSTANT_BUFFER_LIST 매크로에 등록 필요**

---

## Step 3: DoF_Simple_PS.hlsl 생성

**파일**: `Mundi/Shaders/PostProcess/DoF_Simple_PS.hlsl` (신규)

```hlsl
// 텍스처 및 샘플러
Texture2D g_DepthTex : register(t0);
Texture2D g_SceneColorTex : register(t1);
SamplerState g_PointClampSample : register(s0);
SamplerState g_LinearClampSample : register(s1);

// 상수 버퍼
cbuffer DoFParams : register(b2)
{
    float FocalDistance;
    float CocScale;
    float MaxBlurRadius;
    float NearClip;

    float FarClip;
    float TexelSizeX;
    float TexelSizeY;
    float Padding;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 깊이 -> 선형 깊이 변환 (Perspective)
float LinearizeDepth(float depth)
{
    return NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
}

// CoC 계산 (Unreal Engine 방식)
float CalculateCoC(float linearDepth)
{
    // CoC = ((depth - focus) / depth) * CocInfinity
    float coc = ((linearDepth - FocalDistance) / linearDepth) * CocScale;
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// 디스크 블러 (8방향 + 중심)
float4 DiskBlur(float2 uv, float cocRadius)
{
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    const int SAMPLE_COUNT = 8;
    const float2 offsets[SAMPLE_COUNT] = {
        float2(1, 0), float2(0.707, 0.707), float2(0, 1), float2(-0.707, 0.707),
        float2(-1, 0), float2(-0.707, -0.707), float2(0, -1), float2(0.707, -0.707)
    };

    float2 texelSize = float2(TexelSizeX, TexelSizeY);
    float blurRadius = abs(cocRadius);

    // 중심 샘플
    color += g_SceneColorTex.Sample(g_LinearClampSample, uv);
    totalWeight += 1.0;

    // 주변 샘플
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 offset = offsets[i] * blurRadius * texelSize * 1000.0;  // 픽셀 단위로 변환
        color += g_SceneColorTex.Sample(g_LinearClampSample, uv + offset);
        totalWeight += 1.0;
    }

    return color / totalWeight;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // 1. 깊이 샘플링 및 선형화
    float depth = g_DepthTex.Sample(g_PointClampSample, uv).r;
    float linearDepth = LinearizeDepth(depth);

    // 2. CoC 계산
    float coc = CalculateCoC(linearDepth);

    // 3. CoC 크기에 따른 블러
    if (abs(coc) < 0.0001)
    {
        // 초점 영역: 원본 반환
        return g_SceneColorTex.Sample(g_LinearClampSample, uv);
    }
    else
    {
        // 비초점 영역: 블러 적용
        return DiskBlur(uv, coc);
    }
}
```

---

## Step 4: DepthOfFieldPass 구현

**파일**: `Mundi/Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.h` (신규)

```cpp
#pragma once
#include "PostProcessing.h"

class FDepthOfFieldPass : public IPostProcessPass
{
public:
    void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;
};
```

**파일**: `Mundi/Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.cpp` (신규)

```cpp
#include "DepthOfFieldPass.h"
#include "RHI/SwapGuard.h"
#include "RHI/D3D11RHI.h"
#include "RHI/ConstantBufferType.h"
#include "Resource/ResourceManager.h"
#include "Resource/Shader.h"

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // FSwapGuard: 렌더 타겟 스왑 + SRV 자동 해제 (깊이 + 씬컬러 2개)
    FSwapGuard Swap(RHIDevice, 0, 2);

    // 렌더 타겟 설정 (깊이 테스트 없음)
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 셰이더 로드
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance()
        .Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* DoFPS = UResourceManager::GetInstance()
        .Load<UShader>("Shaders/PostProcess/DoF_Simple_PS.hlsl");
    RHIDevice->PrepareShader(FullScreenTriangleVS, DoFPS);

    // SRV 바인딩 (t0: Depth, t1: SceneColor)
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11ShaderResourceView* SRVs[2] = { DepthSRV, SceneSRV };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, SRVs);

    // 샘플러 바인딩 (s0: PointClamp, s1: LinearClamp)
    ID3D11SamplerState* PointClamp = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* LinearClamp = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* Samplers[2] = { PointClamp, LinearClamp };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

    // 상수 버퍼 업데이트
    FDoFBufferType DoFParams;
    DoFParams.FocalDistance = M.Payload.Params0.X;
    DoFParams.CocScale = M.Payload.Params0.Y;
    DoFParams.MaxBlurRadius = M.Payload.Params0.Z;
    DoFParams.NearClip = View->NearClip;
    DoFParams.FarClip = View->FarClip;
    DoFParams.TexelSizeX = 1.0f / RHIDevice->GetViewportWidth();
    DoFParams.TexelSizeY = 1.0f / RHIDevice->GetViewportHeight();
    DoFParams.Padding = 0.0f;

    RHIDevice->SetAndUpdateConstantBuffer(DoFParams);

    // 풀스크린 드로우
    RHIDevice->DrawFullScreenQuad();

    // 커밋
    Swap.Commit();
}
```

---

## Step 5: SceneRenderer 통합

**파일**: `Mundi/Source/Runtime/Renderer/SceneRenderer.h`

**변경 내용**: FDepthOfFieldPass 멤버 추가

```cpp
#include "PostProcessing/DepthOfFieldPass.h"

class FSceneRenderer
{
private:
    // 기존 패스들...
    FHeightFogPass HeightFogPass;
    FFadeInOutPass FadeInOutPass;
    FVignettePass VignettePass;
    FGammaPass GammaPass;
    FDepthOfFieldPass DepthOfFieldPass;  // 추가
};
```

**파일**: `Mundi/Source/Runtime/Renderer/SceneRenderer.cpp`

**변경 내용**: RenderPostProcessingPasses()에 DoF 케이스 추가

```cpp
void FSceneRenderer::RenderPostProcessingPasses()
{
    // ... 기존 코드 (모디파이어 수집 및 정렬) ...

    for (auto& Modifier : PostProcessModifiers)
    {
        switch (Modifier.Type)
        {
        case EPostProcessEffectType::HeightFog:
            HeightFogPass.Execute(Modifier, View, RHIDevice);
            break;
        case EPostProcessEffectType::Fade:
            FadeInOutPass.Execute(Modifier, View, RHIDevice);
            break;
        case EPostProcessEffectType::Vignette:
            VignettePass.Execute(Modifier, View, RHIDevice);
            break;
        case EPostProcessEffectType::Gamma:
            GammaPass.Execute(Modifier, View, RHIDevice);
            break;
        case EPostProcessEffectType::DepthOfField:  // 추가
            DepthOfFieldPass.Execute(Modifier, View, RHIDevice);
            break;
        }
    }
}
```

---

## Step 6: CamMod_DoF.h 생성

**파일**: `Mundi/Source/Runtime/Engine/GameFramework/Camera/CamMod_DoF.h` (신규)

```cpp
#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_DoF : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_DoF, UCameraModifierBase)

    UCamMod_DoF() = default;
    virtual ~UCamMod_DoF() = default;

    // DoF 파라미터
    float FocalDistance = 500.0f;   // 초점 거리 (cm)
    float Fstop = 4.0f;             // f-stop (조리개)
    float FocalLength = 50.0f;      // 렌즈 초점거리 (mm)
    float MaxBlurRadius = 0.02f;    // 최대 블러 (화면 비율)

    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override {}

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override
    {
        if (!bEnabled) return;

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::DepthOfField;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        // CoC 스케일 계산: f^2 / (N * (d - f))
        // f = FocalLength (mm -> cm: *0.1)
        // N = Fstop
        // d = FocalDistance (cm)
        float f = FocalLength * 0.1f;  // mm -> cm
        float CocScale = (f * f) / (Fstop * (FocalDistance - f));

        M.Payload.Params0 = FVector4(FocalDistance, CocScale, MaxBlurRadius, 0.0f);

        Out.Add(M);
    }
};
```

---

## Step 7: PlayerCameraManager 확장

**파일**: `Mundi/Source/Runtime/Engine/GameFramework/PlayerCameraManager.h`

**변경 내용**: DoF API 함수 선언 추가

```cpp
// 헤더 상단에 전방 선언 추가
class UCamMod_DoF;

// public 섹션에 추가
void StartDepthOfField(float FocalDistance, float Fstop = 4.0f, float FocalLength = 50.0f, float MaxBlur = 0.02f);
void StopDepthOfField();
void UpdateDepthOfField(float FocalDistance, float Fstop = 4.0f, float FocalLength = 50.0f, float MaxBlur = 0.02f);
```

**파일**: `Mundi/Source/Runtime/Engine/GameFramework/PlayerCameraManager.cpp`

**변경 내용**: DoF API 함수 구현 추가

```cpp
#include "Camera/CamMod_DoF.h"

void APlayerCameraManager::StartDepthOfField(float FocalDistance, float Fstop, float FocalLength, float MaxBlur)
{
    // 기존 DoF 모디파이어가 있으면 제거
    StopDepthOfField();

    UCamMod_DoF* DoFModifier = new UCamMod_DoF();
    DoFModifier->FocalDistance = FocalDistance;
    DoFModifier->Fstop = Fstop;
    DoFModifier->FocalLength = FocalLength;
    DoFModifier->MaxBlurRadius = MaxBlur;
    DoFModifier->Priority = 5;  // HeightFog(-1) 이후, Vignette 이전
    DoFModifier->bEnabled = true;
    DoFModifier->Weight = 1.0f;

    ActiveModifiers.Add(DoFModifier);
}

void APlayerCameraManager::StopDepthOfField()
{
    for (int32 i = ActiveModifiers.Num() - 1; i >= 0; --i)
    {
        if (ActiveModifiers[i]->IsA<UCamMod_DoF>())
        {
            delete ActiveModifiers[i];
            ActiveModifiers.RemoveAt(i);
        }
    }
}

void APlayerCameraManager::UpdateDepthOfField(float FocalDistance, float Fstop, float FocalLength, float MaxBlur)
{
    for (auto* Modifier : ActiveModifiers)
    {
        if (UCamMod_DoF* DoF = dynamic_cast<UCamMod_DoF*>(Modifier))
        {
            DoF->FocalDistance = FocalDistance;
            DoF->Fstop = Fstop;
            DoF->FocalLength = FocalLength;
            DoF->MaxBlurRadius = MaxBlur;
            return;
        }
    }
    // 없으면 새로 생성
    StartDepthOfField(FocalDistance, Fstop, FocalLength, MaxBlur);
}
```

---

## 테스트 방법

### Lua 스크립트 테스트

```lua
-- DoF 시작 (초점 거리 500cm, f/2.8, 50mm 렌즈)
PlayerCameraManager:StartDepthOfField(500, 2.8, 50, 0.03)

-- DoF 업데이트 (초점 거리만 변경)
PlayerCameraManager:UpdateDepthOfField(1000, 2.8, 50, 0.03)

-- DoF 종료
PlayerCameraManager:StopDepthOfField()
```

### 파라미터 가이드

| 파라미터 | 범위 | 설명 |
|---------|------|------|
| FocalDistance | 10 ~ 100000 cm | 초점 거리. 이 거리의 오브젝트가 선명함 |
| Fstop | 1.2 ~ 32 | 조리개 값. 낮을수록 블러 강함 (f/1.4 = 매우 얕은 심도) |
| FocalLength | 10 ~ 200 mm | 렌즈 초점거리. 길수록 심도가 얕음 |
| MaxBlurRadius | 0.01 ~ 0.1 | 최대 블러 반경 (화면 비율) |

---

## 파일 체크리스트

- [ ] `Mundi/Source/Runtime/Renderer/PostProcessing/PostProcessing.h` - enum 수정
- [ ] `Mundi/Source/Runtime/RHI/ConstantBufferType.h` - FDoFBufferType 추가
- [ ] `Mundi/Shaders/PostProcess/DoF_Simple_PS.hlsl` - 신규 생성
- [ ] `Mundi/Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.h` - 신규 생성
- [ ] `Mundi/Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.cpp` - 신규 생성
- [ ] `Mundi/Source/Runtime/Renderer/SceneRenderer.h` - include 및 멤버 추가
- [ ] `Mundi/Source/Runtime/Renderer/SceneRenderer.cpp` - switch case 추가
- [ ] `Mundi/Source/Runtime/Engine/GameFramework/Camera/CamMod_DoF.h` - 신규 생성
- [ ] `Mundi/Source/Runtime/Engine/GameFramework/PlayerCameraManager.h` - API 선언
- [ ] `Mundi/Source/Runtime/Engine/GameFramework/PlayerCameraManager.cpp` - API 구현
