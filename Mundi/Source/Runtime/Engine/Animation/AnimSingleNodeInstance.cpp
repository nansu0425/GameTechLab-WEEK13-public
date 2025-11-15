#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "SkeletalMeshComponent.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* InAsset, bool bInLooping)
{
    CurrentAsset = InAsset;
    ExtractCtx.bLooping = bInLooping;
    ExtractCtx.PlayRate = 1.f;
    ExtractCtx.bEnableInterpolation = true;
    ExtractCtx.CurrentTime = 0.f;
}

void UAnimSingleNodeInstance::Play(bool bResetTime)
{
    bPlaying = true;
    if (bResetTime)
    {
        ExtractCtx.CurrentTime = 0.f;
    }
}

void UAnimSingleNodeInstance::Stop()
{
    bPlaying = false;
}

void UAnimSingleNodeInstance::SetPlaying(bool bInPlaying)
{
    bPlaying = bInPlaying;
}

void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
    ExtractCtx.bLooping = bInLooping;
}

void UAnimSingleNodeInstance::SetPlayRate(float InRate)
{
    ExtractCtx.PlayRate = InRate;
}

void UAnimSingleNodeInstance::SetPosition(float InSeconds, bool /*bFireNotifies*/)
{
    ExtractCtx.CurrentTime = InSeconds;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTime)
{
    if (!bPlaying || !CurrentAsset)
    {
        return;
    }

    // Advance time using asset length if available
    if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(CurrentAsset))
    {
        const float Length = Seq->GetPlayLength();
        ExtractCtx.Advance(DeltaTime, Length);
    }
}

void UAnimSingleNodeInstance::EvaluateAnimation(FPoseContext& Output)
{
    const FSkeleton* Skeleton = Output.Skeleton;
    if (!Skeleton)
    {
        Output.LocalSpacePose.Empty();
        return;
    }

    UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(CurrentAsset);
    if (!Seq)
    {
        // Fallback: reference pose
        Output.ResetToRefPose();
        return;
    }

    if (!bTreatAssetAsAdditive)
    {
        // Build component-space pose then convert to local for Output
        TArray<FTransform> ComponentPose;
        FAnimationRuntime::ExtractPoseFromSequence(Seq, ExtractCtx, *Skeleton, ComponentPose);

        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, ComponentPose, Output.LocalSpacePose);
    }
    else
    {
        // Local-space additive against reference/bind pose
        // 1) Base pose: start from ref pose
        Output.ResetToRefPose();

        // 2) Extract current and reference component poses
        FAnimExtractContext CurrCtx = ExtractCtx;
        FAnimExtractContext RefCtx = ExtractCtx;  RefCtx.CurrentTime = ReferenceTime;

        TArray<FTransform> CurrComp, RefComp;
        FAnimationRuntime::ExtractPoseFromSequence(Seq, CurrCtx, *Skeleton, CurrComp);
        FAnimationRuntime::ExtractPoseFromSequence(Seq, RefCtx,  *Skeleton, RefComp);

        // 3) Convert to local-space
        TArray<FTransform> CurrLocal, RefLocal;
        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, CurrComp, CurrLocal);
        FAnimationRuntime::ConvertComponentToLocalSpace(*Skeleton, RefComp,  RefLocal);

        // 4) Compute delta (local) per bone: Ref^-1 * Curr  (use relative helper)
        const int32 NumBones = static_cast<int32>(Skeleton->Bones.Num());
        TArray<FTransform> AdditiveDeltaLocal; AdditiveDeltaLocal.SetNum(NumBones);
        for (int32 i = 0; i < NumBones; ++i)
        {
            AdditiveDeltaLocal[i] = RefLocal[i].GetRelativeTransform(CurrLocal[i]);
        }

        // 5) Accumulate onto base pose
        TArray<FTransform> ResultLocal;
        FAnimationRuntime::AccumulateAdditivePose(*Skeleton, Output.LocalSpacePose, AdditiveDeltaLocal, 1.f, ResultLocal);
        Output.LocalSpacePose = ResultLocal;
    }
}

