#pragma once
#include "CameraModifierBase.h"

class UCamMod_Fade : public UCameraModifierBase
{
public:
    FLinearColor FadeColor = FLinearColor::Zero();
    float        TargetAmount = 1.0f; // 0~1
    float        CurrentOpacity = 0.0f;
    float        Speed = 1.0f; // 초당 변화량

    virtual void ApplyToView(float DeltaTime, FSceneView& InOutView) override
    {
        // 전처리에서 타임라인 업데이트만 (뷰 행렬 수정 없음)
        //if (FMath::IsNearlyZero(DeltaTime) || FMath::IsNearlyZero(Speed))
        //{
        //    return;
        //}

        const float ClampedTarget = FMath::Clamp(TargetAmount, 0.f, 1.f);
        TargetAmount = ClampedTarget;

        CurrentOpacity = FMath::Clamp(CurrentOpacity + Speed * DeltaTime, 0.f, 1.f);

        const bool bReachedTarget = (Speed >= 0.f && CurrentOpacity >= TargetAmount) ||
                                    (Speed < 0.f && CurrentOpacity <= TargetAmount);
        if (bReachedTarget)
        {
            CurrentOpacity = TargetAmount;
        }
    }

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out, const FSceneView&) override
    {
        if (!bEnabled || Weight <= 0.f || CurrentOpacity <= 0.f) return;

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::Fade;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        M.Payload.Color = FadeColor;
        M.Payload.Params0 = FVector4(CurrentOpacity, 0, 0, 0); // Params0.x = FadeAmount

        Out.Add(M);
    }
};


