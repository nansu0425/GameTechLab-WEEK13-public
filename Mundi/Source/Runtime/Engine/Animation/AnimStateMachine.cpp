#include "pch.h"
#include "AnimStateMachine.h"
#include "AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "AnimSequenceBase.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequencePlayer.h"

int32 FAnimNode_StateMachine::AddState(const FAnimState& State, UAnimSequenceBase* Sequence)
{
    const int32 Index = States.Add(State);
    // Wire authoring fields into the internal sequence player for this state
    FAnimState& S = States[Index];
    S.Player.SetSequence(Sequence);
    S.Player.SetPlayRate(S.PlayRate);
    S.Player.SetLooping(S.bLooping);
    S.Player.SetInterpolationEnabled(true);
    return Index;
}

void FAnimNode_StateMachine::AddTransition(const FAnimTransition& Transition)
{
    Transitions.Add(Transition);
}

int32 FAnimNode_StateMachine::FindStateByName(const FString& Name) const
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

void FAnimNode_StateMachine::SetCurrentState(int32 StateIndex, float BlendTime)
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
        // Reset current state's player time/settings
        FAnimState& Curr = States[StateIndex];
        Curr.Player.GetExtractContext() = FAnimExtractContext{};
        Curr.Player.SetPlayRate(Curr.PlayRate);
        Curr.Player.SetLooping(Curr.bLooping);
        Curr.Player.SetInterpolationEnabled(true);
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
    // Reset next state's player time/settings
    FAnimState& Next = States[StateIndex];
    Next.Player.GetExtractContext() = FAnimExtractContext{};
    Next.Player.SetPlayRate(Next.PlayRate);
    Next.Player.SetLooping(Next.bLooping);
    Next.Player.SetInterpolationEnabled(true);
}

const FAnimState* FAnimNode_StateMachine::GetStateChecked(int32 Index) const
{
    if (Index < 0 || Index >= States.Num()) return nullptr;
    return &States[Index];
}

void FAnimNode_StateMachine::Update(FAnimationBaseContext& Context)
{
    const float DeltaSeconds = Context.GetDeltaSeconds();

    // Advance current/next players
    if (FAnimState* Curr = (Runtime.CurrentState >= 0 && Runtime.CurrentState < States.Num()) ? &States[Runtime.CurrentState] : nullptr)
    {
        Curr->Player.Update(Context);
    }
    if (FAnimState* Next = (Runtime.NextState >= 0 && Runtime.NextState < States.Num()) ? &States[Runtime.NextState] : nullptr)
    {
        Next->Player.Update(Context);

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
            Runtime.NextState = -1;
            Runtime.BlendAlpha = 0.f;
            Runtime.BlendDuration = 0.f;
        }
    }
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.Skeleton;
    if (!Skeleton)
    {
        Output.LocalSpacePose.Empty();
        return;
    }

    // Evaluate current and optional next via sequence players
    FPoseContext PoseA; PoseA.Initialize(Output.GetComponent(), Skeleton, Output.GetDeltaSeconds());
    FPoseContext PoseB; PoseB.Initialize(Output.GetComponent(), Skeleton, Output.GetDeltaSeconds());

    FAnimState* Curr = (Runtime.CurrentState >= 0 && Runtime.CurrentState < States.Num()) ? &States[Runtime.CurrentState] : nullptr;
    FAnimState* Next = (Runtime.NextState >= 0 && Runtime.NextState < States.Num()) ? &States[Runtime.NextState] : nullptr;

    if (Curr)
    {
        Curr->Player.Evaluate(PoseA);
    }
    else
    {
        PoseA.ResetToRefPose();
    }

    if (Next)
    {
        Next->Player.Evaluate(PoseB);
        TArray<FTransform> CompA, CompB, CompOut;
        FAnimationRuntime::ConvertLocalToComponentSpace(*Skeleton, PoseA.LocalSpacePose, CompA);
        FAnimationRuntime::ConvertLocalToComponentSpace(*Skeleton, PoseB.LocalSpacePose, CompB);
        const float Alpha = std::clamp(Runtime.BlendAlpha, 0.f, 1.f);
        FAnimationRuntime::BlendTwoPoses(*Skeleton, CompA, CompB, Alpha, CompOut);
        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CompOut, Output.LocalSpacePose);
    }
    else
    {
        Output.LocalSpacePose = PoseA.LocalSpacePose;
    }
}




