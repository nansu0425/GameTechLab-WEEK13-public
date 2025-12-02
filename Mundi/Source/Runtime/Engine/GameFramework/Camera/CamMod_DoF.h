#pragma once
#include "CameraModifierBase.h"

// Depth of Field 카메라 모디파이어
// PlayerCameraManager::StartDepthOfField()로 생성
class UCamMod_DoF : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_DoF, UCameraModifierBase)

    UCamMod_DoF() = default;
    virtual ~UCamMod_DoF() = default;

    // DoF 파라미터 (CameraComponent에서 계산된 값 전달받음)
    float FocalDistance = 5.0f;     // 초점 거리 (m)
    float CocScale = 0.0f;          // 계산된 CoC 스케일 (CameraComponent::GetDepthOfFieldCocScale())
    float MaxBlurRadius = 10.0f;    // 최대 블러 반경 (픽셀)

    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override {}
    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override;
};
