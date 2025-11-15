#include "pch.h"
#include "AnimationRuntime.h"
#include "AnimNodeBase.h"
#include "Vector.h"
#include "VertexData.h"

// Helpers
static void BuildLocalBindPoseFromSkeleton(const FSkeleton& Skeleton, TArray<FTransform>& OutLocalPose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    OutLocalPose.SetNum(NumBones);
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FBone& ThisBone = Skeleton.Bones[BoneIndex];
        const int32 ParentIndex = ThisBone.ParentIndex;
        FMatrix LocalBindMatrix;
        if (ParentIndex == -1)
        {
            LocalBindMatrix = ThisBone.BindPose;
        }
        else
        {
            const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
            LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
        }
        OutLocalPose[BoneIndex] = FTransform(LocalBindMatrix);
    }
}

void FAnimationRuntime::ConvertLocalToComponentSpace(const FSkeleton& Skeleton, const TArray<FTransform>& LocalPose,
    TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    OutComponentPose.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            OutComponentPose[BoneIndex] = LocalPose[BoneIndex];
        }
        else
        {
            OutComponentPose[BoneIndex] = OutComponentPose[ParentIndex].GetWorldTransform(LocalPose[BoneIndex]);
        }
    }
}

void FAnimationRuntime::ConvertComponentToLocalSpace(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPose,
    TArray<FTransform>& OutLocalPose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    OutLocalPose.SetNum(NumBones);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            OutLocalPose[BoneIndex] = ComponentPose[BoneIndex];
        }
        else
        {
            OutLocalPose[BoneIndex] = ComponentPose[ParentIndex].GetRelativeTransform(ComponentPose[BoneIndex]);
        }
    }
}

void FAnimationRuntime::ExtractPoseFromSequence(const UAnimSequenceBase* Sequence, const FAnimExtractContext& ExtractContext,
    const FSkeleton& Skeleton, TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    if (NumBones <= 0)
    {
        OutComponentPose.Empty();
        return;
    }

    // 1) Extract local pose
    TArray<FTransform> LocalPose;
    LocalPose.SetNum(NumBones);

    if (Sequence)
    {
        // Assume teammate provides this API:
        // void ExtractBonePose(const FSkeleton& Skeleton, float Time, bool bLooping, bool bInterpolate, TArray<FTransform>& OutLocalPose) const;
        Sequence->ExtractBonePose(Skeleton, ExtractContext.CurrentTime, ExtractContext.bLooping, ExtractContext.bEnableInterpolation, LocalPose);
    }
    else
    {
        // Fallback to reference/bind local pose
        BuildLocalBindPoseFromSkeleton(Skeleton, LocalPose);
    }

    // 2) Convert to component space
    ConvertLocalToComponentSpace(Skeleton, LocalPose, OutComponentPose);
}

void FAnimationRuntime::BlendTwoPoses(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPoseA, const TArray<FTransform>& ComponentPoseB,
    float Alpha, TArray<FTransform>& OutComponentPose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    OutComponentPose.SetNum(NumBones);

    const float ClampedAlpha = std::clamp(Alpha, 0.f, 1.f);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& A = ComponentPoseA[BoneIndex];
        const FTransform& B = ComponentPoseB[BoneIndex];
        OutComponentPose[BoneIndex] = FTransform::Lerp(A, B, ClampedAlpha);
    }
}

void FAnimationRuntime::AccumulateAdditivePose(const FSkeleton& Skeleton, const TArray<FTransform>& BasePose,
    const TArray<FTransform>& AdditivePose, float Weight, TArray<FTransform>& OutAdditivePose)
{
    const int32 NumBones = static_cast<int32>(Skeleton.Bones.Num());
    OutAdditivePose.SetNum(NumBones);

    const float W = std::max(0.f, Weight);
    const FQuat Identity(0.f, 0.f, 0.f, 1.f);

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& Base = BasePose[BoneIndex];
        const FTransform& Delta = AdditivePose[BoneIndex];

        // Rotation: apply weighted delta on top of base
        FQuat RDeltaWeighted = FQuat::Slerp(Identity, Delta.Rotation, W);
        FQuat OutR = RDeltaWeighted * Base.Rotation;
        OutR.Normalize();

        // Translation: linear offset
        FVector OutT = Base.Translation + Delta.Translation * W;

        // Scale: lerp from 1 to delta then component-wise multiply
        const FVector One(1.f, 1.f, 1.f);
        FVector SWeight = FVector::Lerp(One, Delta.Scale3D, W);
        FVector OutS = FVector(Base.Scale3D.X * SWeight.X, Base.Scale3D.Y * SWeight.Y, Base.Scale3D.Z * SWeight.Z);

        OutAdditivePose[BoneIndex] = FTransform(OutT, OutR, OutS);
    }
}

void FAnimationRuntime::NormalizeRotations(TArray<FTransform>& InComponentPose)
{
    for (auto& T : InComponentPose)
    {
        T.Rotation.Normalize();
    }
}

