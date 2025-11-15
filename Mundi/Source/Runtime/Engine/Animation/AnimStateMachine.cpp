#include "pch.h"
#include "AnimStateMachine.h"
#include "AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "AnimSequenceBase.h"
#include "SkeletalMeshComponent.h"

int32 UAnimStateMachineInstance::AddState(const FAnimState& State)
{
    return States.Add(State);
}

void UAnimStateMachineInstance::AddTransition(const FAnimTransition& Transition)
{
    Transitions.Add(Transition);
}

int32 UAnimStateMachineInstance::FindStateByName(const FString& Name) const
{
    for (int32 i = 0; i < States.Num(); ++i)
    {
        if (States[i].Name == Name)
        {
            return i;
        }
    }
    return -1;
}

void UAnimStateMachineInstance::SetCurrentState(int32 StateIndex, float BlendTime)
{
    if (StateIndex < 0 || StateIndex >= States.Num())
    {
        // Invalid target, cancel any transition
        Runtime.NextState = -1;
        Runtime.BlendAlpha = 0.f;
        Runtime.BlendDuration = 0.f;
        return;
    }

    if (Runtime.CurrentState == -1 || BlendTime <= 0.f)
    {
        // Immediate switch
        Runtime.CurrentState = StateIndex;
        Runtime.NextState = -1;
        Runtime.BlendAlpha = 0.f;
        Runtime.BlendDuration = 0.f;
        // initialize sampling context for current state
        Runtime.CtxA = FAnimExtractContext{};
        Runtime.CtxA.CurrentTime = 0.f;
        Runtime.CtxA.PlayRate = States[StateIndex].PlayRate;
        Runtime.CtxA.bLooping = States[StateIndex].bLooping;
        Runtime.CtxA.bEnableInterpolation = true;
        Runtime.CtxB = FAnimExtractContext{};
        return;
    }

    if (Runtime.CurrentState == StateIndex)
    {
        // No-op
        return;
    }

    // Begin transition
    Runtime.NextState = StateIndex;
    Runtime.BlendDuration = std::max(0.f, BlendTime);
    Runtime.BlendAlpha = 0.f;
    // initialize next state's sampling context
    Runtime.CtxB = FAnimExtractContext{};
    Runtime.CtxB.CurrentTime = 0.f;
    Runtime.CtxB.PlayRate = States[StateIndex].PlayRate;
    Runtime.CtxB.bLooping = States[StateIndex].bLooping;
    Runtime.CtxB.bEnableInterpolation = true;
}

const FAnimState* UAnimStateMachineInstance::GetStateChecked(int32 Index) const
{
    if (Index < 0 || Index >= States.Num()) return nullptr;
    return &States[Index];
}

void UAnimStateMachineInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    // Advance current state time
    if (const FAnimState* Curr = GetStateChecked(Runtime.CurrentState))
    {
        const float Length = Curr->Asset ? Curr->Asset->GetPlayLength() : 0.f;
        Runtime.CtxA.Advance(DeltaSeconds, Length);
    }

    // If blending, advance next state time and blend alpha
    if (const FAnimState* Next = GetStateChecked(Runtime.NextState))
    {
        const float Length = Next->Asset ? Next->Asset->GetPlayLength() : 0.f;
        Runtime.CtxB.Advance(DeltaSeconds, Length);

        // Advance blend alpha
        if (Runtime.BlendDuration <= 0.f)
        {
            Runtime.BlendAlpha = 1.f;
        }
        else
        {
            Runtime.BlendAlpha += DeltaSeconds / Runtime.BlendDuration;
        }

        if (Runtime.BlendAlpha >= 1.f)
        {
            // Finalize transition
            Runtime.BlendAlpha = 1.f;
            Runtime.CurrentState = Runtime.NextState;
            Runtime.CtxA = Runtime.CtxB;
            Runtime.NextState = -1;
            Runtime.BlendAlpha = 0.f;
            Runtime.BlendDuration = 0.f;
            Runtime.CtxB = FAnimExtractContext{};
        }
    }
}

void UAnimStateMachineInstance::EvaluateAnimation(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.Skeleton;
    if (!Skeleton)
    {
        Output.LocalSpacePose.Empty();
        return;
    }

    // Build component poses for current and (optional) next states
    TArray<FTransform> CompA, CompB, CompOut;

    const FAnimState* Curr = GetStateChecked(Runtime.CurrentState);
    const FAnimState* Next = GetStateChecked(Runtime.NextState);

    // Extract A
    if (Curr && Curr->Asset)
    {
        UAnimSequenceBase* SeqA = Cast<UAnimSequenceBase>(Curr->Asset);
        FAnimationRuntime::ExtractPoseFromSequence(SeqA, Runtime.CtxA, *Skeleton, CompA);
    }
    else
    {
        // Fallback to bind/component ref pose via runtime helper
        FAnimationRuntime::ExtractPoseFromSequence(nullptr, FAnimExtractContext{}, *Skeleton, CompA);
    }

    // If blending, extract B and blend; otherwise just use A
    if (Next && Next->Asset)
    {
        UAnimSequenceBase* SeqB = Cast<UAnimSequenceBase>(Next->Asset);
        FAnimationRuntime::ExtractPoseFromSequence(SeqB, Runtime.CtxB, *Skeleton, CompB);

        const float Alpha = std::clamp(Runtime.BlendAlpha, 0.f, 1.f);
        FAnimationRuntime::BlendTwoPoses(*Skeleton, CompA, CompB, Alpha, CompOut);
    }
    else
    {
        CompOut = CompA;
    }

    // Convert to local-space for output
    FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CompOut, Output.LocalSpacePose);
}

bool UAnimStateMachineInstance::IsPlaying() const
{
    // Consider playing if we have a valid current state or are mid-transition
    return (Runtime.CurrentState != -1) || (Runtime.NextState != -1);
}
