#pragma once
#include "AnimInstance.h"
#include "UAnimStateMachineInstance.generated.h"

class UAnimationAsset;

// Simple runtime animation state machine types
struct FAnimState
{
    FString Name;
    UAnimationAsset* Asset = nullptr;
    float PlayRate = 1.f;
    bool bLooping = true;
};

struct FAnimTransition
{
    int32 FromStateIndex = -1;
    int32 ToStateIndex = -1;
    float BlendTime = 0.2f;
    bool bAutomatic = false;
};

struct FAnimExtractContext; // fwd decl

struct FAnimSMRuntime
{
    int32 CurrentState = -1;
    int32 NextState = -1;
    float BlendAlpha = 0.f;
    float BlendDuration = 0.f;
    FAnimExtractContext CtxA; // runtime sampling context for current state
    FAnimExtractContext CtxB; // runtime sampling context for next state (during transition)
};

UCLASS(DisplayName="애님 상태 머신 인스턴스", Description="경량 상태 전이/블렌드 플레이어")
class UAnimStateMachineInstance : public UAnimInstance
{
public:
    GENERATED_REFLECTION_BODY()

    UAnimStateMachineInstance() = default;

    // Authoring API
    int32 AddState(const FAnimState& State);
    void AddTransition(const FAnimTransition& Transition);
    void SetCurrentState(int32 StateIndex, float BlendTime = 0.f);
    int32 FindStateByName(const FString& Name) const;

    // UAnimInstance overrides
    void NativeUpdateAnimation(float DeltaSeconds) override;
    void EvaluateAnimation(FPoseContext& Output) override;
    bool IsPlaying() const override;

private:
    const FAnimState* GetStateChecked(int32 Index) const;

private:
    TArray<FAnimState> States;
    TArray<FAnimTransition> Transitions;
    FAnimSMRuntime Runtime;
};


