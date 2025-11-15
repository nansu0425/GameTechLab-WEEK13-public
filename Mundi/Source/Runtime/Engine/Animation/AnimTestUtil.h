#pragma once

class UAnimSequence;
class UAnimStateMachineInstance;
class USkeletalMeshComponent;
struct FSkeleton;

namespace AnimTestUtil
{
    // Creates a simple procedural sequence: root (bone 0) swings around X.
    // If Skeleton has >1 bones, optionally adds a mild swing on bone 1.
    UAnimSequence* CreateSimpleSwingSequence(const FSkeleton& Skeleton, float LengthSeconds = 1.0f, float FrameRate = 30.0f);

    // Convenience: Build a 2-state machine ("Idle", "Walk") from procedural sequences and attach to component.
    // Returns the created state machine instance. Requires component to have a valid skeletal mesh + skeleton.
    UAnimStateMachineInstance* SetupTwoStateSMOnComponent(USkeletalMeshComponent* Component,
                                                         float IdleLengthSeconds = 1.0f,
                                                         float WalkLengthSeconds = 0.6f,
                                                         float FrameRate = 30.0f);

    // Helper to trigger a state change by name with blend time.
    void TriggerTransition(UAnimStateMachineInstance* SM, const FString& ToStateName, float BlendTime = 0.25f);
}

