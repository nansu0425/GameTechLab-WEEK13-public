#pragma once
#include "AnimInstance.h"

class UAnimationAsset;
class UAnimSequenceBase;

class UAnimSingleNodeInstance : public UAnimInstance
{
public:
    UAnimSingleNodeInstance() = default;

    // Control API
    void SetAnimationAsset(UAnimationAsset* InAsset, bool bInLooping = true);
    void Play(bool bResetTime = true);
    void Stop();
    void SetPlaying(bool bInPlaying);
    void SetLooping(bool bInLooping);
    void SetPlayRate(float InRate);
    void SetPosition(float InSeconds, bool bFireNotifies = false);
    float GetPosition() const { return CurrentTime; }

    // UAnimInstance overrides
    void NativeUpdateAnimation(float DeltaTime) override;
    void EvaluateAnimation(FPoseContext& Output) override;

    // Additive options (optional for now)
    void SetTreatAsAdditive(bool bInAdditive) { bTreatAssetAsAdditive = bInAdditive; }
    void SetAdditiveType(EAdditiveType InType) { AdditiveType = InType; }
    void SetReferenceTime(float InRefTime) { ReferenceTime = InRefTime; }

private:
    UAnimationAsset* CurrentAsset = nullptr;
    float CurrentTime = 0.f;
    float PlayRate = 1.f;
    bool bLooping = true;
    bool bPlaying = false;

    // Additive
    bool bTreatAssetAsAdditive = false;
    EAdditiveType AdditiveType = EAdditiveType::LocalSpace;
    float ReferenceTime = 0.f;
};

