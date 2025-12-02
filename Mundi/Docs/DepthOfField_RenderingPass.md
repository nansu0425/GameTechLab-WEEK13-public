# Depth of Field 렌더링 패스 분석

## 개요

Mundi 엔진의 Depth of Field (DoF) 포스트 프로세스 효과의 렌더링 파이프라인을 문서화합니다.

---

## 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DoF 데이터 흐름                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐                                                       │
│  │ UCameraComponent │ ◄── UPROPERTY (에디터에서 설정)                        │
│  │                  │                                                       │
│  │ bEnableDoF       │                                                       │
│  │ FocalDistance    │                                                       │
│  │ Fstop            │                                                       │
│  │ FocalLength      │                                                       │
│  │ MaxBlurRadius    │                                                       │
│  └────────┬─────────┘                                                       │
│           │                                                                 │
│           ▼                                                                 │
│  ┌────────────────────────────────────────────────────────────────────┐     │
│  │                        모드별 분기                                  │     │
│  ├────────────────────────┬───────────────────────────────────────────┤     │
│  │      Pilot 모드        │              PIE 모드                     │     │
│  │  (FViewportClient)     │      (APlayerCameraManager)               │     │
│  ├────────────────────────┼───────────────────────────────────────────┤     │
│  │ 매 프레임 직접 읽음     │ BeginPlay()에서 StartDepthOfField() 호출  │     │
│  │ FPostProcessModifier   │ UCamMod_DoF 생성                          │     │
│  │ 직접 생성              │ ActiveModifiers 배열에 추가               │     │
│  │                        │                                           │     │
│  │ 실시간 반영 O          │ 초기화 시점 값 고정                       │     │
│  └────────────┬───────────┴─────────────────┬─────────────────────────┘     │
│               │                             │                               │
│               ▼                             ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                     FPostProcessModifier                             │   │
│  │  Type = EPostProcessEffectType::DepthOfField                         │   │
│  │  Payload.Params0 = (FocalDistance, CocScale, MaxBlurRadius, 0)       │   │
│  └──────────────────────────────────────────┬───────────────────────────┘   │
│                                             │                               │
│                                             ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                     FSceneView.Modifiers                             │   │
│  │              (TArray<FPostProcessModifier>)                          │   │
│  └──────────────────────────────────────────┬───────────────────────────┘   │
│                                             │                               │
│                                             ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                      FSceneRenderer                                  │   │
│  │              RenderPostProcessingPasses()                            │   │
│  │                          │                                           │   │
│  │                          ▼                                           │   │
│  │              switch(Modifier.Type)                                   │   │
│  │              case DepthOfField:                                      │   │
│  │                  DepthOfFieldPass.Execute()                          │   │
│  └──────────────────────────────────────────┬───────────────────────────┘   │
│                                             │                               │
│                                             ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    FDepthOfFieldPass                                 │   │
│  │                                                                      │   │
│  │  1. FSwapGuard 생성 (Ping-Pong 버퍼 스왑)                            │   │
│  │  2. 셰이더 로드 (FullScreenTriangle_VS + DoF_Simple_PS)              │   │
│  │  3. SRV 바인딩 (t0:Depth, t1:SceneColor)                             │   │
│  │  4. 상수 버퍼 업데이트 (FDoFBufferType → b2)                         │   │
│  │  5. DrawFullScreenQuad()                                             │   │
│  │  6. Swap.Commit()                                                    │   │
│  └──────────────────────────────────────────┬───────────────────────────┘   │
│                                             │                               │
│                                             ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    DoF_Simple_PS.hlsl                                │   │
│  │                                                                      │   │
│  │  1. 깊이 샘플링 → LinearizeDepth()                                   │   │
│  │  2. CoC 계산 → CalculateCoC()                                        │   │
│  │  3. 조건 분기:                                                       │   │
│  │     - |CoC| < 0.5px → 원본 반환 (초점 영역)                          │   │
│  │     - |CoC| >= 0.5px → DiskBlur() (비초점 영역)                      │   │
│  │  4. 최종 색상 출력                                                   │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 핵심 파일

| 파일 | 역할 |
|------|------|
| `Components/CameraComponent.h` | DoF 파라미터 UPROPERTY 정의, CoC 스케일 계산 |
| `GameFramework/Camera/CamMod_DoF.h/.cpp` | DoF 카메라 모디파이어 클래스 |
| `GameFramework/PlayerCameraManager.cpp` | PIE 모드 DoF API (Start/Stop/Update) |
| `Renderer/FViewportClient.cpp` | Pilot 모드 DoF 처리 |
| `Renderer/PostProcessing/DepthOfFieldPass.h/.cpp` | DoF 렌더 패스 구현 |
| `Renderer/PostProcessing/PostProcessing.h` | FPostProcessModifier, EPostProcessEffectType 정의 |
| `RHI/ConstantBufferType.h` | FDoFBufferType 상수 버퍼 정의 |
| `Shaders/PostProcess/DoF_Simple_PS.hlsl` | DoF 픽셀 셰이더 |

---

## 1. 파라미터 정의

### UCameraComponent (에디터 노출 파라미터)

```cpp
// Source/Runtime/Engine/Components/CameraComponent.h

UPROPERTY(EditAnywhere, Category="DepthOfField")
bool bEnableDepthOfField = false;

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="0.0, 1000.0")
float DepthOfFieldFocalDistance = 5.0f;  // 초점 거리 (m)

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="1.2, 32.0")
float DepthOfFieldFstop = 4.0f;  // f-stop (조리개) - 낮을수록 블러 강함

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="10.0, 200.0")
float DepthOfFieldFocalLength = 50.0f;  // 렌즈 초점거리 (mm)

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="1.0, 50.0")
float DepthOfFieldMaxBlurRadius = 10.0f;  // 최대 블러 반경 (픽셀)
```

### CoC 스케일 계산

```cpp
// CameraComponent.h - GetDepthOfFieldCocScale()
// CoC_∞ = f² / (N × (d - f))
// f = 초점 거리 (mm → m), N = f-stop, d = 피사체 거리 (m)

float GetDepthOfFieldCocScale() const
{
    float FocalLengthM = DepthOfFieldFocalLength * 0.001f;  // mm → m
    float Denominator = DepthOfFieldFstop * (DepthOfFieldFocalDistance - FocalLengthM);
    if (Denominator <= 0.0001f) return 0.0f;
    return (FocalLengthM * FocalLengthM) / Denominator;
}
```

---

## 2. 모드별 DoF 활성화 경로

### Pilot 모드 (에디터 카메라 Pilot)

```cpp
// Source/Runtime/Renderer/FViewportClient.cpp:191-216

if (bPilotCameraMode && PilotCameraComponent)
{
    FSceneView RenderView(PilotCameraComponent, Viewport, &World->GetRenderSettings());

    // Pilot 카메라의 DoF가 활성화된 경우 Modifier 직접 생성
    if (PilotCameraComponent->IsDepthOfFieldEnabled())
    {
        FPostProcessModifier DoFMod;
        DoFMod.Type = EPostProcessEffectType::DepthOfField;
        DoFMod.Priority = 0;
        DoFMod.bEnabled = true;
        DoFMod.Weight = 1.0f;
        DoFMod.Payload.Params0 = FVector4(
            PilotCameraComponent->GetDepthOfFieldFocalDistance(),
            PilotCameraComponent->GetDepthOfFieldCocScale(),
            PilotCameraComponent->GetDepthOfFieldMaxBlurRadius(),
            0.0f
        );
        RenderView.Modifiers.Add(DoFMod);
    }

    Renderer->RenderSceneForView(World, &RenderView, Viewport);
}
```

**특징**: CameraComponent의 UPROPERTY 값을 **매 프레임 직접 읽음** → 실시간 반영

### PIE 모드 (Play In Editor)

```cpp
// Source/Runtime/Engine/GameFramework/PlayerCameraManager.cpp:71-92

void APlayerCameraManager::BeginPlay()
{
    Super::BeginPlay();

    CurrentViewCamera = GetWorld()->FindComponent<UCameraComponent>();
    if (CurrentViewCamera)
    {
        // PIE 시작 시 CameraComponent의 DoF 설정이 활성화되어 있으면 Modifier 자동 생성
        if (CurrentViewCamera->IsDepthOfFieldEnabled())
        {
            StartDepthOfField(
                CurrentViewCamera->DepthOfFieldFocalDistance,
                CurrentViewCamera->DepthOfFieldFstop,
                CurrentViewCamera->DepthOfFieldFocalLength,
                CurrentViewCamera->DepthOfFieldMaxBlurRadius
            );
        }
    }
}
```

**특징**: BeginPlay() 시점에 **한 번만 초기화** → 이후 API로만 제어 (실시간 반영 X)

---

## 3. UCamMod_DoF 카메라 모디파이어

```cpp
// Source/Runtime/Engine/GameFramework/Camera/CamMod_DoF.h

class UCamMod_DoF : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_DoF, UCameraModifierBase)

    float FocalDistance = 5.0f;     // 초점 거리 (m)
    float CocScale = 0.0f;          // 계산된 CoC 스케일
    float MaxBlurRadius = 10.0f;    // 최대 블러 반경 (픽셀)

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override;
};
```

```cpp
// Source/Runtime/Engine/GameFramework/Camera/CamMod_DoF.cpp

void UCamMod_DoF::CollectPostProcess(TArray<FPostProcessModifier>& Out)
{
    if (!bEnabled) return;

    FPostProcessModifier M;
    M.Type = EPostProcessEffectType::DepthOfField;
    M.Priority = Priority;
    M.bEnabled = true;
    M.Weight = Weight;
    M.SourceObject = this;

    // Payload.Params0: X=FocalDistance, Y=CocScale, Z=MaxBlurRadius
    M.Payload.Params0 = FVector4(FocalDistance, CocScale, MaxBlurRadius, 0.0f);

    Out.Add(M);
}
```

---

## 4. DoF API (PlayerCameraManager)

```cpp
// Source/Runtime/Engine/GameFramework/PlayerCameraManager.cpp

// DoF 시작 - 새 Modifier 생성
void APlayerCameraManager::StartDepthOfField(
    float FocalDistance,
    float Fstop,
    float FocalLength,
    float MaxBlurRadius,
    int32 InPriority)
{
    StopDepthOfField();  // 기존 DoF 제거

    if (CurrentViewCamera)
    {
        // CameraComponent 파라미터 설정 (CoC 계산에 필요)
        CurrentViewCamera->bEnableDepthOfField = true;
        CurrentViewCamera->DepthOfFieldFocalDistance = FocalDistance;
        CurrentViewCamera->DepthOfFieldFstop = Fstop;
        CurrentViewCamera->DepthOfFieldFocalLength = FocalLength;
        CurrentViewCamera->DepthOfFieldMaxBlurRadius = MaxBlurRadius;

        UCamMod_DoF* DoFMod = new UCamMod_DoF();
        DoFMod->Priority = InPriority;
        DoFMod->bEnabled = true;
        DoFMod->Duration = -1.0f;  // 무한 지속
        DoFMod->FocalDistance = FocalDistance;
        DoFMod->CocScale = CurrentViewCamera->GetDepthOfFieldCocScale();
        DoFMod->MaxBlurRadius = MaxBlurRadius;
        ActiveModifiers.Add(DoFMod);
    }
}

// DoF 중지 - Modifier 비활성화
void APlayerCameraManager::StopDepthOfField()
{
    if (CurrentViewCamera)
        CurrentViewCamera->bEnableDepthOfField = false;

    for (UCameraModifierBase* M : ActiveModifiers)
    {
        if (UCamMod_DoF* DoFMod = Cast<UCamMod_DoF>(M))
        {
            DoFMod->Duration = 0.0f;
            DoFMod->bEnabled = false;
            break;
        }
    }
}

// DoF 파라미터 업데이트
void APlayerCameraManager::UpdateDepthOfField(
    float FocalDistance,
    float Fstop,
    float FocalLength,
    float MaxBlurRadius)
{
    if (CurrentViewCamera)
    {
        CurrentViewCamera->DepthOfFieldFocalDistance = FocalDistance;
        // ... 파라미터 업데이트

        for (UCameraModifierBase* M : ActiveModifiers)
        {
            if (UCamMod_DoF* DoFMod = Cast<UCamMod_DoF>(M))
            {
                DoFMod->FocalDistance = FocalDistance;
                DoFMod->CocScale = CurrentViewCamera->GetDepthOfFieldCocScale();
                DoFMod->MaxBlurRadius = MaxBlurRadius;
                break;
            }
        }
    }
}
```

---

## 5. 렌더 패스 실행

### FDepthOfFieldPass

```cpp
// Source/Runtime/Renderer/PostProcessing/DepthOfFieldPass.cpp

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M) || !View) return;

    // 1) Ping-Pong 버퍼 스왑 (깊이 + 씬 컬러 2개 SRV 언바인드)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);

    // 2) 렌더 타겟 설정 (깊이 없이 SceneColor만)
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 3) 셰이더 로드
    UShader* FullScreenTriangleVS = Load("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* DoFPS = Load("Shaders/PostProcess/DoF_Simple_PS.hlsl");
    RHIDevice->PrepareShader(FullScreenTriangleVS, DoFPS);

    // 4) SRV 바인딩
    // t0: SceneDepth, t1: SceneColorSource
    ID3D11ShaderResourceView* Srvs[2] = { DepthSRV, SceneSRV };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

    // 5) 샘플러 바인딩
    // s0: PointClamp (깊이용), s1: LinearClamp (컬러용)
    ID3D11SamplerState* Smps[2] = { PointClampSampler, LinearClampSampler };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);

    // 6) 상수 버퍼 업데이트
    // b0: PostProcessCB (Near/Far)
    // b2: DoFCB (DoF 파라미터)
    FDoFBufferType DoFConstant;
    DoFConstant.FocalDistance = M.Payload.Params0.X;
    DoFConstant.CocScale = M.Payload.Params0.Y;
    DoFConstant.MaxBlurRadius = M.Payload.Params0.Z;
    DoFConstant.NearClip = View->NearClip;
    DoFConstant.FarClip = View->FarClip;
    DoFConstant.TexelSizeX = 1.0f / ViewportWidth;
    DoFConstant.TexelSizeY = 1.0f / ViewportHeight;
    RHIDevice->SetAndUpdateConstantBuffer(DoFConstant);

    // 7) 풀스크린 드로우
    RHIDevice->DrawFullScreenQuad();

    // 8) 스왑 확정
    Swap.Commit();
}
```

---

## 6. HLSL 셰이더

### DoF_Simple_PS.hlsl

```hlsl
// Shaders/PostProcess/DoF_Simple_PS.hlsl

// 텍스처
Texture2D g_DepthTex : register(t0);
Texture2D g_SceneColorTex : register(t1);

// 샘플러
SamplerState g_PointClampSample : register(s0);   // 깊이용
SamplerState g_LinearClampSample : register(s1);  // 컬러용

// 상수 버퍼
cbuffer DoFCB : register(b2)
{
    float FocalDistance;      // 초점 거리 (m)
    float CocScale;           // CoC 스케일 (무한대 CoC)
    float MaxBlurRadius;      // 최대 블러 반경 (픽셀)
    float NearClip;
    float FarClip;
    float TexelSizeX;         // 1/Width
    float TexelSizeY;         // 1/Height
    float Padding;
}

// 깊이 → 선형 깊이 변환 (Perspective)
float LinearizeDepth(float depth)
{
    return NearClip * FarClip / (FarClip - depth * (FarClip - NearClip));
}

// Circle of Confusion 계산
// CoC = ((depth - focusDistance) / depth) × CocScale
float CalculateCoC(float linearDepth)
{
    float coc = ((linearDepth - FocalDistance) / max(linearDepth, 0.001f)) * CocScale;
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// 8방향 디스크 블러
float4 DiskBlur(float2 uv, float cocRadius)
{
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;

    static const int SAMPLE_COUNT = 8;
    static const float2 offsets[SAMPLE_COUNT] = {
        float2(1.0, 0.0),   float2(0.707, 0.707),
        float2(0.0, 1.0),   float2(-0.707, 0.707),
        float2(-1.0, 0.0),  float2(-0.707, -0.707),
        float2(0.0, -1.0),  float2(0.707, -0.707)
    };

    float2 texelSize = float2(TexelSizeX, TexelSizeY);
    float blurRadius = abs(cocRadius);

    // 중심 샘플 (가중치 2배)
    color += g_SceneColorTex.Sample(g_LinearClampSample, uv) * 2.0;
    totalWeight += 2.0;

    // 주변 8방향 샘플
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 offset = offsets[i] * blurRadius * texelSize;
        color += g_SceneColorTex.Sample(g_LinearClampSample, uv + offset);
        totalWeight += 1.0;
    }

    return color / totalWeight;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // 1. 깊이 샘플링
    float depth = g_DepthTex.Sample(g_PointClampSample, uv).r;

    // 스카이박스 (depth >= 0.9999) → 최대 블러
    if (depth >= 0.9999f)
        return DiskBlur(uv, MaxBlurRadius);

    // 2. 선형 깊이 변환
    float linearDepth = LinearizeDepth(depth);

    // 3. CoC 계산
    float coc = CalculateCoC(linearDepth);

    // 4. CoC 크기에 따른 처리
    if (abs(coc) < 0.5f)  // 초점 영역
        return g_SceneColorTex.Sample(g_LinearClampSample, uv);
    else                  // 비초점 영역
        return DiskBlur(uv, coc);
}
```

---

## 7. 상수 버퍼 레이아웃

### FDoFBufferType (register b2)

```cpp
// Source/Runtime/RHI/ConstantBufferType.h

struct alignas(16) FDoFBufferType
{
    float FocalDistance;      // offset 0  - 초점 거리 (m)
    float CocScale;           // offset 4  - CoC 스케일
    float MaxBlurRadius;      // offset 8  - 최대 블러 반경 (px)
    float NearClip;           // offset 12 - Near 클리핑

    float FarClip;            // offset 16 - Far 클리핑
    float TexelSizeX;         // offset 20 - 1/Width
    float TexelSizeY;         // offset 24 - 1/Height
    float Padding;            // offset 28 - 16바이트 정렬용
};
// 총 32바이트 (16바이트 정렬 만족)

CONSTANT_BUFFER_INFO(FDoFBufferType, 2, false, true)  // slot=b2, PS only
```

---

## 8. 포스트 프로세스 통합

### EPostProcessEffectType

```cpp
// Source/Runtime/Renderer/PostProcessing/PostProcessing.h

enum class EPostProcessEffectType : uint8
{
    HeightFog,
    Vignette,
    Bloom,
    Fade,
    Gamma,
    DepthOfField,  // DoF
};
```

### FPostProcessModifier

```cpp
struct FPostProcessModifier
{
    EPostProcessEffectType Type;
    int32      Priority;          // 정렬 순서
    bool       bEnabled;
    float      Weight;            // 블렌드 가중치
    UObject*   SourceObject;      // 디버그용 소스
    FPostProcessPayload Payload;  // 범용 파라미터 슬롯
};

struct FPostProcessPayload
{
    FVector4 Params0;  // DoF: (FocalDistance, CocScale, MaxBlurRadius, 0)
    FVector4 Params1;
    FLinearColor Color;
};
```

### SceneRenderer 통합

```cpp
// Source/Runtime/Renderer/SceneRenderer.cpp

void FSceneRenderer::RenderPostProcessingPasses(...)
{
    for (auto& Modifier : Modifiers)
    {
        switch (Modifier.Type)
        {
        // ... 다른 효과들 ...
        case EPostProcessEffectType::DepthOfField:
            DepthOfFieldPass.Execute(Modifier, View, RHIDevice);
            break;
        }
    }
}
```

---

## 9. Modifier 수명 관리

DoF Modifier는 다른 Modifier들(Fade, Shake, Vignette)과 동일한 패턴으로 관리됩니다.

| 속성 | 의미 |
|------|------|
| `Duration >= 0` | 해당 시간 경과 후 자동 삭제 |
| `Duration < 0` | 무한 지속 (명시적 `StopDepthOfField()` 필요) |
| `bEnabled = false` | 다음 BuildForFrame()에서 삭제 대상 |

```cpp
// PlayerCameraManager::BuildForFrame() 수명 정리 로직
for (int32 i = ActiveModifiers.Num()-1; i >= 0; i--)
{
    UCameraModifierBase* M = ActiveModifiers[i];
    M->TickLifetime(DeltaTime);

    // Duration >= 0 && bEnabled == false → 자동 삭제
    if (M->Duration >= 0.f && !M->bEnabled)
    {
        delete M;
        ActiveModifiers.RemoveAtSwap(i);
    }
}
```

---

## 10. 파라미터 가이드

| 파라미터 | 타입 | 범위 | 설명 |
|----------|------|------|------|
| `FocalDistance` | float | 0.1 ~ 1000.0 (m) | 초점 거리. 이 거리의 오브젝트가 선명함 |
| `Fstop` | float | 1.2 ~ 32.0 | 조리개 값. 낮을수록 심도가 얕음 (블러 강함) |
| `FocalLength` | float | 10 ~ 200 (mm) | 렌즈 초점거리. 길수록 심도가 얕음 |
| `MaxBlurRadius` | float | 1 ~ 50 (px) | 최대 블러 반경. 높을수록 극단적 블러 |

### 예시 설정

| 시나리오 | FocalDistance | Fstop | FocalLength | MaxBlurRadius |
|----------|---------------|-------|-------------|---------------|
| 인물 촬영 (얕은 심도) | 3.0 | 1.4 | 85 | 15 |
| 풍경 촬영 (깊은 심도) | 50.0 | 11.0 | 24 | 5 |
| 매크로 (극도로 얕은 심도) | 0.3 | 2.8 | 100 | 25 |

---

## 제한사항 (Phase 1)

1. **단일 패스**: 분리된 near/far 블러 없음 (haloing 아티팩트 가능)
2. **고정 샘플 수**: 8방향 + 중심 = 9샘플 (품질 제한)
3. **전경/배경 구분 없음**: CoC 부호만으로 처리
4. **성능 최적화 없음**: 풀 해상도 처리
