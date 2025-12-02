# Depth of Field (DoF) 구현 분석 - Unreal Engine 참조

## 개요

이 문서는 Unreal Engine의 Depth of Field 구현 방식을 분석하여 Mundi 엔진에 DoF 기능을 구현하기 위한 참조 자료입니다.

---

## 1. 핵심 카메라 파라미터

Unreal Engine DoF에서 사용하는 3가지 핵심 카메라 파라미터:

| 파라미터 | UE 변수명 | 단위 | 설명 |
|---------|----------|------|------|
| **Focus Distance** | `DepthOfFieldFocalDistance` | cm (Unreal Unit) | 카메라가 초점을 맞추는 거리 |
| **Aperture (F-Stop)** | `DepthOfFieldFstop` | f-number | 렌즈 조리개 값 (f/2.8, f/4 등) |
| **Focal Length** | FOV에서 계산 | mm | 렌즈의 초점 거리 |

### 1.1 추가 파라미터

| 파라미터 | UE 변수명 | 기본값 | 설명 |
|---------|----------|-------|------|
| Sensor Width | `DepthOfFieldSensorWidth` | 24.576mm | 카메라 센서 너비 (APS-C) |
| Blade Count | `DepthOfFieldBladeCount` | 8 | 조리개 날 개수 (보케 모양) |
| Min F-Stop | `DepthOfFieldMinFstop` | 1.2 | 최대 개구부 (보케 곡률 제어) |

---

## 2. Circle of Confusion (CoC) 계산

### 2.1 핵심 공식

Unreal Engine에서 사용하는 CoC 계산 공식:

```
CoC_infinity = f² / (N × (d - f))
```

여기서:
- `f` = Focal Length (초점 거리, mm)
- `N` = F-Stop (조리개 값)
- `d` = Focus Distance (초점 거리, mm)

### 2.2 픽셀별 CoC 계산

**셰이더 코드 (DOFCommon.ush:209-226):**

```hlsl
float SceneDepthToCocRadius(float SceneDepth)
{
    const float Focus = View.DepthOfFieldFocalDistance;

    // 기본 CoC 계산: 초점 거리와의 차이 비율
    float CocRadius = ((SceneDepth - Focus) / SceneDepth) * CocInfinityRadius;

    // Depth blur 적용
    float DepthBlurAbsRadius = (1.0 - exp2(-SceneDepth * DepthBlurExponent)) * DepthBlurRadius;

    float ReturnCoc = max(abs(CocRadius), DepthBlurAbsRadius) * sign(CocRadius);

    return clamp(ReturnCoc, CocMinRadius, CocMaxRadius);
}
```

### 2.3 CoC 부호 규칙

- **양수 CoC**: 배경 (Focus Distance보다 먼 곳)
- **음수 CoC**: 전경 (Focus Distance보다 가까운 곳)
- **0 CoC**: 초점 영역 (선명함)

---

## 3. 카메라 파라미터 → CoC 모델 변환

### 3.1 FPhysicalCocModel 구조체

**파일:** `DiaphragmDOF.h:28-116`

```cpp
struct FPhysicalCocModel
{
    // 센서 크기 (Unreal Unit)
    float SensorWidth;
    float SensorHeight;

    // 초점 거리 (Unreal Unit, FOV에서 계산)
    float VerticalFocalLength;

    // 조리개 값
    float FStops;

    // 배경 무한대 CoC 반경 (ViewportUV 단위)
    float InfinityBackgroundCocRadius;

    // 초점 거리 (Unreal Unit)
    float FocusDistance;

    // 스퀴즈 팩터 (아나모르픽 렌즈)
    float Squeeze;

    // CoC 제한값
    float MinForegroundCocRadius;  // 음수
    float MaxBackgroundCocRadius;  // 양수
};
```

### 3.2 Compile 함수 (핵심 변환 로직)

**파일:** `DiaphragmDOFUtils.cpp:102-199`

```cpp
void FPhysicalCocModel::Compile(const FViewInfo& View)
{
    // 1. 카메라 파라미터 가져오기
    FocusDistance = View.FinalPostProcessSettings.DepthOfFieldFocalDistance;
    FStops = View.FinalPostProcessSettings.DepthOfFieldFstop;

    // 2. 센서 크기 계산 (mm → Unreal Unit)
    const float MMToUU = 0.1f;  // 1mm = 0.1cm
    SensorWidth = View.FinalPostProcessSettings.DepthOfFieldSensorWidth * MMToUU;

    // 3. FOV에서 Focal Length 계산
    const float VerticalHalfFOV = FMath::Atan(1.0f / View.ViewMatrices.GetProjectionMatrix().M[1][1]);
    VerticalFocalLength = 0.5f * SensorHeight * (1.0f / FMath::Tan(VerticalHalfFOV));

    // 4. 무한대 CoC 계산
    // coc = f² / (N × (d - f))
    if (FStops > 0.f && FocusDistance > 0.f)
    {
        float VerticalDiameter = FMath::Square(VerticalFocalLength) /
                                 (FStops * (FocusDistance - VerticalFocalLength));

        // 센서 좌표계로 변환
        float UncroppedVerticalInfinityBackgroundCocRadius = VerticalDiameter * 0.5f / SensorHeight;

        // 화면 좌표계로 변환
        InfinityBackgroundCocRadius = VerticalInfinityBackgroundCocRadius / RenderingAspectRatio;
    }
}
```

---

## 4. FOV에서 Focal Length 계산

**파일:** `DiaphragmDOFUtils.cpp:28-44`

```cpp
float ComputeFocalLengthFromFov(const FSceneView& View)
{
    // FOV → Focal Length 변환 공식:
    // fov = 2 × atan(d / (2 × f))
    // f = 0.5 × d × (1 / tan(fov/2))

    float const d = View.FinalPostProcessSettings.DepthOfFieldSensorWidth;  // 센서 폭 (mm)
    float const HalfFOV = FMath::Atan(1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0]);
    float const FocalLength = 0.5f * d * (1.0f / FMath::Tan(HalfFOV));  // mm

    return FocalLength;
}
```

---

## 5. DoF 렌더링 파이프라인

### 5.1 전체 흐름

```
┌─────────────────────────────────────────────────────────────────┐
│  1. Setup Pass                                                  │
│     - Scene Color + Scene Depth 읽기                            │
│     - 픽셀별 CoC 계산 (SceneDepthToCocRadius)                   │
│     - Full-res 또는 Half-res 출력                               │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  2. CoC Tile Flatten/Dilate                                     │
│     - 타일 기반 CoC Min/Max 계산                                │
│     - 이웃 타일로 CoC 확장 (bleeding 방지)                       │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  3. Gather Pass (Background/Foreground)                         │
│     - 링 기반 샘플링 (3-5 rings)                                │
│     - CoC 가중 블러                                             │
│     - 배경/전경 별도 처리                                        │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  4. Scatter Pass (선택적, Bokeh DoF)                            │
│     - 밝은 픽셀을 스프라이트로 산란                              │
│     - 보케 LUT 사용                                             │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  5. Recombine Pass                                              │
│     - Full-res 원본과 블러 결과 합성                            │
│     - Slight out-of-focus 처리                                  │
│     - 전경/배경 레이어 블렌딩                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 핵심 셰이더 파일

| 파일 | 역할 |
|------|------|
| `DOFCommon.ush` | CoC 계산 함수, 공통 상수 |
| `DOFSetup.usf` | Setup 패스 (CoC 계산 + 다운샘플) |
| `DOFGatherPass.usf` | 링 기반 Gather 블러 |
| `DOFRecombine.usf` | 최종 합성 |
| `DOFBokehLUT.usf` | 보케 형태 LUT 생성 |

---

## 6. Mundi 엔진 구현 가이드

### 6.1 UCameraComponent 확장

```cpp
// CameraComponent.h에 추가할 DoF 파라미터
UPROPERTY(EditAnywhere, Category="DepthOfField")
float DepthOfFieldFocalDistance = 0.0f;  // cm, 0이면 DoF 비활성화

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="1.2, 32.0")
float DepthOfFieldFstop = 4.0f;  // f/4 기본값

UPROPERTY(EditAnywhere, Category="DepthOfField", Range="4.0, 200.0")
float DepthOfFieldFocalLength = 50.0f;  // mm, 또는 FOV에서 계산

UPROPERTY(EditAnywhere, Category="DepthOfField")
float DepthOfFieldSensorWidth = 24.576f;  // mm, APS-C 센서
```

### 6.2 FSceneView 확장

```cpp
// SceneView.h에 추가할 DoF 데이터
struct FDepthOfFieldParams
{
    float FocalDistance;           // 초점 거리 (cm)
    float FocalLength;             // 렌즈 초점거리 (UU)
    float Aperture;                // 조리개 직경 (UU)
    float SensorWidth;             // 센서 폭 (UU)
    float CocInfinityRadius;       // 무한대 CoC (normalized)
    float CocMaxRadius;            // 최대 CoC (화면 비율)
    float CocMinRadius;            // 최소 CoC (화면 비율, 음수)
};
```

### 6.3 PostProcessing 확장

```cpp
// PostProcessing.h에 추가
enum class EPostProcessEffectType : uint8
{
    HeightFog,
    Vignette,
    Bloom,
    Fade,
    Gamma,
    DepthOfField,  // 추가
};

// DoF 전용 페이로드
struct FDoFPayload
{
    float FocalDistance;
    float CocInfinityRadius;
    float CocMaxRadius;
    float CocMinRadius;
};
```

### 6.4 셰이더 구현

**DoF_Setup.hlsl (CoC 계산):**

```hlsl
cbuffer DoFParams : register(b0)
{
    float FocalDistance;        // 초점 거리 (world units)
    float CocInfinityRadius;    // 무한대 CoC (화면 비율)
    float CocMaxRadius;         // 최대 배경 CoC
    float CocMinRadius;         // 최소 전경 CoC (음수)
};

// 깊이 → CoC 변환
float DepthToCocRadius(float SceneDepth)
{
    // 핵심 공식: CoC = ((depth - focus) / depth) × CocInfinity
    float CocRadius = ((SceneDepth - FocalDistance) / SceneDepth) * CocInfinityRadius;
    return clamp(CocRadius, CocMinRadius, CocMaxRadius);
}
```

**DoF_Blur.hlsl (가우시안 블러):**

```hlsl
// 간단한 구현: CoC 기반 가우시안 블러
float4 DoFBlurPS(float2 UV : TEXCOORD0) : SV_Target
{
    float CenterCoc = CocTexture.Sample(Sampler, UV).r;
    float BlurRadius = abs(CenterCoc) * MaxBlurPixels;

    float4 Color = 0;
    float TotalWeight = 0;

    // 링 기반 샘플링 (Unreal 방식)
    for (int ring = 0; ring < RingCount; ring++)
    {
        float RingRadius = (ring + 1) * BlurRadius / RingCount;
        int SampleCount = max(1, ring * 6);  // 링당 샘플 수

        for (int s = 0; s < SampleCount; s++)
        {
            float Angle = 2.0 * PI * s / SampleCount;
            float2 Offset = float2(cos(Angle), sin(Angle)) * RingRadius * TexelSize;

            float SampleCoc = CocTexture.Sample(Sampler, UV + Offset).r;
            float4 SampleColor = SceneColor.Sample(Sampler, UV + Offset);

            // CoC 비교로 오클루전 처리
            float Weight = (abs(SampleCoc) >= abs(CenterCoc) - 0.5) ? 1.0 : 0.0;
            Color += SampleColor * Weight;
            TotalWeight += Weight;
        }
    }

    return Color / max(TotalWeight, 1.0);
}
```

---

## 7. 구현 우선순위

### Phase 1: 기본 DoF (필수)
1. UCameraComponent에 DoF 파라미터 추가
2. CoC 계산 셰이더 구현
3. 간단한 가우시안 블러 DoF

### Phase 2: 품질 개선 (선택)
1. 분리 가능 블러 (Separable Blur) - 성능 최적화
2. 전경/배경 분리 처리 - 품질 개선
3. Half-resolution 처리 - 성능 최적화

### Phase 3: 고급 기능 (선택)
1. 보케 형태 시뮬레이션
2. Scatter 기반 DoF (밝은 점광원)
3. 조리개 날 시뮬레이션

---

## 8. 참조 파일 목록

### Unreal Engine 소스 코드

| 파일 경로 | 내용 |
|----------|------|
| `Engine/Source/Runtime/Renderer/Private/PostProcess/DiaphragmDOF.h` | FPhysicalCocModel 정의 |
| `Engine/Source/Runtime/Renderer/Private/PostProcess/DiaphragmDOFUtils.cpp` | CoC 모델 Compile 함수 |
| `Engine/Source/Runtime/Renderer/Private/PostProcess/DiaphragmDOF.cpp` | 렌더 패스 구현 |
| `Engine/Shaders/Private/DiaphragmDOF/DOFCommon.ush` | SceneDepthToCocRadius 함수 |
| `Engine/Shaders/Private/DiaphragmDOF/DOFSetup.usf` | Setup 패스 셰이더 |
| `Engine/Shaders/Private/PostProcessDOF.usf` | 레거시 DoF (Gaussian) |

### Mundi 엔진 대상 파일

| 파일 경로 | 수정 내용 |
|----------|----------|
| `Mundi/Source/Runtime/Engine/Components/CameraComponent.h` | DoF 파라미터 추가 |
| `Mundi/Source/Runtime/Renderer/SceneView.h` | FDepthOfFieldParams 추가 |
| `Mundi/Source/Runtime/Renderer/PostProcessing/PostProcessing.h` | DoF 타입 추가 |
| `Mundi/Shaders/` | DoF 셰이더 파일 생성 |

---

## 9. 핵심 수식 요약

```
┌─────────────────────────────────────────────────────────────────┐
│  FOV → Focal Length                                             │
│  f = 0.5 × SensorWidth × (1 / tan(HalfFOV))                    │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  무한대 CoC (Infinity Background CoC)                           │
│  CoC_∞ = f² / (N × (d - f))                                    │
│                                                                 │
│  f = Focal Length                                               │
│  N = F-Stop                                                     │
│  d = Focus Distance                                             │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  픽셀별 CoC                                                     │
│  CoC(depth) = ((depth - FocusDistance) / depth) × CoC_∞        │
│                                                                 │
│  CoC > 0: 배경 (블러)                                           │
│  CoC < 0: 전경 (블러)                                           │
│  CoC ≈ 0: 초점 (선명)                                           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  조리개 직경                                                    │
│  ApertureDiameter = FocalLength / FStop                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 10. 일반적인 카메라 설정 예시

| 시나리오 | Focus Distance | F-Stop | Focal Length | 효과 |
|---------|---------------|--------|--------------|------|
| 인물 촬영 | 300cm | f/2.8 | 85mm | 매우 얕은 DoF |
| 풍경 | 10000cm | f/11 | 24mm | 넓은 초점 범위 |
| 매크로 | 50cm | f/4 | 100mm | 극도로 얕은 DoF |
| 게임 기본 | 500cm | f/4 | 50mm | 중간 DoF |
