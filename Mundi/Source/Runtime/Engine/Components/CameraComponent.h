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

    // CoC 스케일 계산: CoC_∞ = f² / (N × (d - f))
    // f = 초점 거리 (mm → m), N = f-stop, d = 피사체 거리 (m)
    float GetDepthOfFieldCocScale() const
    {
        float FocalLengthM = DepthOfFieldFocalLength * 0.001f;  // mm → m
        float Denominator = DepthOfFieldFstop * (DepthOfFieldFocalDistance - FocalLengthM);
        if (Denominator <= 0.0001f) return 0.0f;
        return (FocalLengthM * FocalLengthM) / Denominator;
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

