#include "pch.h"
#include "AnimTestUtil.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "VertexData.h"

static void FillTrackKeys(FRawAnimSequenceTrack& Track, float Time, float Amp)
{
    // Simple harmonic rotation around X, small translation wobble, unit scale
    const float angle = std::sinf(Time * 2.0f * 3.14159265f) * Amp; // radians
    FQuat rot = FQuat::FromAxisAngle(FVector(1, 0, 0), angle);
    rot.Normalize();

    Track.PositionKeys.Add(FVector(0.f, 0.f, 0.f));
    Track.RotationKeys.Add(rot);
    Track.ScaleKeys.Add(FVector(1.f, 1.f, 1.f));
}

UAnimSequence* AnimTestUtil::CreateSimpleSwingSequence(const FSkeleton& Skeleton, float LengthSeconds, float FrameRate)
{
    UAnimSequence* Seq = NewObject<UAnimSequence>();
    if (!Seq)
    {
        return nullptr;
    }

    UAnimDataModel* Model = NewObject<UAnimDataModel>();
    if (!Model)
    {
        return Seq;
    }

    Model->SequenceLength = LengthSeconds;
    Model->FrameRate = FrameRate;

    const int32 NumFrames = FMath::Max(2, static_cast<int32>(LengthSeconds * FrameRate));
    Model->NumberOfFrames = NumFrames;
    Model->BoneAnimationTracks.Empty();

    // Track for bone 0 (root)
    if (Skeleton.Bones.Num() > 0)
    {
        FBoneAnimationTrack RootTrack(0, Skeleton.Bones[0].Name);
        RootTrack.InternalTrack.PositionKeys.Reserve(NumFrames);
        RootTrack.InternalTrack.RotationKeys.Reserve(NumFrames);
        RootTrack.InternalTrack.ScaleKeys.Reserve(NumFrames);

        for (int32 i = 0; i < NumFrames; ++i)
        {
            const float t = (static_cast<float>(i) / FrameRate);
            FillTrackKeys(RootTrack.InternalTrack, t / LengthSeconds, 0.5f);
        }
        Model->BoneAnimationTracks.Add(RootTrack);
    }

    // Optional second bone swing
    if (Skeleton.Bones.Num() > 1)
    {
        FBoneAnimationTrack ChildTrack(1, Skeleton.Bones[1].Name);
        ChildTrack.InternalTrack.PositionKeys.Reserve(NumFrames);
        ChildTrack.InternalTrack.RotationKeys.Reserve(NumFrames);
        ChildTrack.InternalTrack.ScaleKeys.Reserve(NumFrames);

        for (int32 i = 0; i < NumFrames; ++i)
        {
            const float t = (static_cast<float>(i) / FrameRate);
            FillTrackKeys(ChildTrack.InternalTrack, t / LengthSeconds, 0.25f);
        }
        Model->BoneAnimationTracks.Add(ChildTrack);
    }

    Seq->SetAnimDataModel(Model);
    return Seq;
}

