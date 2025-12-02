#pragma once

#include "SceneComponent.h"
#include "UCameraComponent.generated.h"

UCLASS(DisplayName="카메라 컴포넌트", Description="카메라 뷰포트 컴포넌트입니다")
class UCameraComponent : public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY()

    UCameraComponent();

protected:
    ~UCameraComponent() override;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Camera", Range="1.0, 179.0")
    float FieldOfView;   // degrees

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.1, 10.0")
    float AspectRatio;  //사용x

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.01, 1000.0")
    float NearClip;

    UPROPERTY(EditAnywhere, Category="Camera", Range="1.0, 100000.0")
    float FarClip;

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.1, 10.0")
    float ZoomFactor;

    // ===== Depth of Field 파라미터 =====
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

    UPROPERTY(EditAnywhere, Category="DepthOfField", Range="1.0, 100.0")
    float DepthOfFieldSensorWidth = 36.0f;  // 센서 너비 (mm) - Full Frame 기본값

    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    // Projection settings
    void SetFOV(float NewFOV) { FieldOfView = NewFOV; }
    void SetAspectRatio(float NewAspect) { AspectRatio = NewAspect; }
    void SetClipPlanes(float NewNear, float NewFar) { NearClip = NewNear; FarClip = NewFar; }
    void SetNearClipPlane(float NewNear) { NearClip = NewNear; }
    void SetFarClipPlane(float NewFar) { FarClip = NewFar; }
    void SetProjectionMode(ECameraProjectionMode Mode) { ProjectionMode = Mode; }
    void SetZoomFactor(float InZoomFactor) { ZoomFactor = InZoomFactor; };
    
    float GetFOV() const { return FieldOfView; }
    float GetAspectRatio() const { return AspectRatio; }
    float GetNearClip() const { return NearClip; }
    float GetFarClip() const { return FarClip; }
    float GetZoomFactor()const { return ZoomFactor; };
    ECameraProjectionMode GetProjectionMode() const { return ProjectionMode; }

    // ===== Depth of Field Getters =====
    bool IsDepthOfFieldEnabled() const { return bEnableDepthOfField; }
    float GetDepthOfFieldFocalDistance() const { return DepthOfFieldFocalDistance; }
    float GetDepthOfFieldMaxBlurRadius() const { return DepthOfFieldMaxBlurRadius; }

    // CoC 스케일 계산 (정규화된 값, 화면 높이 기준 0~1)
    // 물리 공식: CoC_∞ = f² / (N × (d - f))
    // 센서 정규화: CoC_normalized = CoC_∞ / SensorHeight
    // f = 초점 거리 (mm), N = f-stop, d = 피사체 거리 (mm로 변환)
    float GetDepthOfFieldCocScale() const
    {
        // 모든 단위를 mm로 통일
        float FocalLengthMM = DepthOfFieldFocalLength;  // 이미 mm
        float FocalDistanceMM = DepthOfFieldFocalDistance * 1000.0f;  // m → mm
        float SensorHeightMM = DepthOfFieldSensorWidth / AspectRatio;  // 센서 높이 (mm)

        float Denominator = DepthOfFieldFstop * (FocalDistanceMM - FocalLengthMM);
        if (Denominator <= 0.0001f) return 0.0f;

        // 물리적 CoC (mm 단위)
        float CocMM = (FocalLengthMM * FocalLengthMM) / Denominator;

        // 센서 높이 기준으로 정규화 (0~1 범위)
        // 셰이더에서 ViewportHeight를 곱하면 픽셀 단위가 됨
        return CocMM / SensorHeightMM;
    }

    // Matrices
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio) const; //사용 x
    FMatrix GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const; //ViewportAspectRatio는 Viewport에서 얻어올 수 있음

    void SetViewGizmo()
    {
        bSetViewGizmo = true;
    }
    const TArray<FVector>& GetViewGizmo() const
    {
        return ViewGizmo;
    }
    // Directions in world space
    FVector GetForward() const;
    FVector GetRight() const;
    FVector GetUp() const;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Serialization
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // 카메라 기즈모 가시성 제어 (Pilot 모드용)
    void SetCameraGizmoVisible(bool bVisible);

private:





    ECameraProjectionMode ProjectionMode;

    bool bSetViewGizmo;
    TArray<FVector> ViewGizmo;

    class UStaticMeshComponent* CameraGizmo = nullptr;
};

