#include "pch.h"
#include "AnimStateMachineInstance.h"
#include "SkeletalMeshComponent.h"

void UAnimStateMachineInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    USkeletalMeshComponent* Comp = GetOwningComponent();
    if (!Comp) return;
    const USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    if (!Skeleton) return;

    FAnimationBaseContext Ctx;
    Ctx.Initialize(Comp, Skeleton, DeltaSeconds);
    StateMachine.Update(Ctx);
}

void UAnimStateMachineInstance::EvaluateAnimation(FPoseContext& Output)
{
    StateMachine.Evaluate(Output);
}

