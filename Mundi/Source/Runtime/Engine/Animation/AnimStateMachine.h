#pragma once
#include "AnimNodeBase.h"
#include "AnimSequencePlayer.h"

class UAnimSequenceBase;

// Simple runtime animation state machine types
struct FAnimState
{
    // Authoring inputs (descriptor only)
    FString Name;
    float PlayRate = 1.f;
    bool bLooping = true;

    // Internal player node (owns the sequence and playback state)
    FAnimNode_SequencePlayer Player;
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
};

// Graph node: state machine
struct FAnimNode_StateMachine : public FAnimNode_Base
{
    // Authoring API
    int32 AddState(const FAnimState& State, UAnimSequenceBase* Sequence);
    void AddTransition(const FAnimTransition& Transition);
    void SetCurrentState(int32 StateIndex, float BlendTime = 0.f);
    int32 FindStateByName(const FString& Name) const;

    // FAnimNode_Base overrides
    void Update(FAnimationBaseContext& Context) override;
    void Evaluate(FPoseContext& Output) override;

    bool IsActive() const { return (Runtime.CurrentState != -1) || (Runtime.NextState != -1); }

private:
    const FAnimState* GetStateChecked(int32 Index) const;

private:
    TArray<FAnimState> States;
    TArray<FAnimTransition> Transitions;
    FAnimSMRuntime Runtime;
};
