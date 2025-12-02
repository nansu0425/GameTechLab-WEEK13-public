#include "pch.h"
#include "CamMod_DoF.h"
#include "PostProcessing/PostProcessing.h"

IMPLEMENT_CLASS(UCamMod_DoF)

void UCamMod_DoF::CollectPostProcess(TArray<FPostProcessModifier>& Out)
{
    if (!bEnabled) return;

    FPostProcessModifier M;
    M.Type = EPostProcessEffectType::DepthOfField;
    M.Priority = Priority;
    M.bEnabled = true;
    M.Weight = Weight;
    M.SourceObject = this;

    // CoC 스케일은 CameraComponent::GetDepthOfFieldCocScale()에서 계산된 값 사용
    // Payload.Params0: X=FocalDistance, Y=CocScale, Z=MaxBlurRadius
    M.Payload.Params0 = FVector4(FocalDistance, CocScale, MaxBlurRadius, 0.0f);

    Out.Add(M);
}
