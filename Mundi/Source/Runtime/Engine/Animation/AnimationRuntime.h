#pragma once

struct FAnimExtractContext;
class UAnimSequenceBase;

class FAnimationRuntime
{
public:
    // pose building/conversion
    static void ConvertLocalToComponentSpace(const FSkeleton& Skeleton, const TArray<FTransform>& LocalPose,
        TArray<FTransform>& OutComponentPose);
    static void ConvertComponentToLocalSpace(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPose,
        TArray<FTransform>& OutLocalPose);

    // extraction
    static void ExtractPoseFromSequence(const UAnimSequenceBase* Sequence, const FAnimExtractContext& ExtractContext,
        const FSkeleton& Skeleton, TArray<FTransform>& OutComponentPose);

    // blending
    static void BlendTwoPoses(const FSkeleton& Skeleton, const TArray<FTransform>& ComponentPoseA, const TArray<FTransform>& ComponentPoseB,
        float Alpha, TArray<FTransform>& OutComponentPose);
    static void AccumulateAdditivePose(const FSkeleton& Skeleton, const TArray<FTransform>& BasePose,
        const TArray<FTransform>& AdditivePose, float Weight, TArray<FTransform>& OutAdditivePose);

    static void NormalizeRotations(TArray<FTransform>& InComponentPose);
};
