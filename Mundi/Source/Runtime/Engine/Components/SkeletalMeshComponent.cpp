#include "pch.h"
#include "SkeletalMeshComponent.h"

#include "AnimNodeBase.h"
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // 테스트용 기본 메시 설정
    SetSkeletalMesh(GDataDir + "/Test.fbx"); 
}


void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    // Drive animation instance if present
    if (bUseAnimation && AnimInstance && SkeletalMesh && SkeletalMesh->GetSkeleton())
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);

        FPoseContext OutputPose;
        OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
        AnimInstance->EvaluateAnimation(OutputPose);

        // Apply local-space pose to component and rebuild skinning
        CurrentLocalSpacePose = OutputPose.LocalSpacePose;
        ForceRecomputePose();
        return; // skip test code when animation is active
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);
        TempFinalSkinningNormalMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix); 
        }
        
        ForceRecomputePose();

        // Rebind anim instance to new skeleton
        if (AnimInstance)
        {
            AnimInstance->InitializeAnimation(this);
        }
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
        TempFinalSkinningNormalMatrices.Empty();
    }
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->InitializeAnimation(this);
    }
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* Asset, bool bLooping, float InPlayRate)
{
    UAnimSingleNodeInstance* Single = nullptr;
    if (!AnimInstance)
    {
        Single = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(Single);
    }
    else
    {
        Single = Cast<UAnimSingleNodeInstance>(AnimInstance);
        if (!Single)
        {
            // Replace with a single-node instance for simple playback
            Single = NewObject<UAnimSingleNodeInstance>();
            SetAnimInstance(Single);
        }
    }

    if (Single)
    {
        Single->SetAnimationAsset(Asset, bLooping);
        Single->SetPlayRate(InPlayRate);
        Single->Play(true);
    }
}

void USkeletalMeshComponent::StopAnimation()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->Stop();
    }
}

void USkeletalMeshComponent::SetAnimationPosition(float InSeconds)
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->SetPosition(InSeconds, false);
    }
}

bool USkeletalMeshComponent::IsPlayingAnimation() const
{
    return AnimInstance ? AnimInstance->IsPlaying() : false;
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}


FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();
    // ComponentSpace -> Final Skinning Matrices 계산
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
    PerformSkinning();
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();
        
        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
        TempFinalSkinningNormalMatrices[BoneIndex] = TempFinalSkinningMatrices[BoneIndex].Inverse().Transpose();
    }
}

