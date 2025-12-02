#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "PlatformTime.h"
#include "AnimSequence.h"
#include "FbxLoader.h"
#include "AnimNodeBase.h"
#include "AnimSingleNodeInstance.h"
#include "AnimStateMachineInstance.h"
#include "AnimBlendSpaceInstance.h"
#include "BodySetup.h"
#include "PhysicsAsset.h"
#include "PhysScene.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // Keep constructor lightweight for editor/viewer usage.
    // Load a simple default test mesh if available; viewer UI can override.
    SetSkeletalMesh(GDataDir + "/DancingRacer.fbx");
    // TODO - 애니메이션 나중에 써먹으세요
    /*
	UAnimationAsset* AnimationAsset = UResourceManager::GetInstance().Get<UAnimSequence>("Data/DancingRacer_mixamo.com");
    PlayAnimation(AnimationAsset, true, 1.f);
    */
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    ClearBodies();
    ClearConstraints();
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();
    PhysicsTest();
    // UBodySetup* BodySetup = NewObject<UBodySetup>();
    // FSphylElem SPhylElem(5.0f, 10.0f);
    // BodySetup->AggGeom.SphylElems.Add(SPhylElem);
    // FBodyInstance& Body = GetBodyInstanceRef();
    // Body.InitBody(BodySetup, GetWorldTransform(), this, GetWorld()->GetPhysScene());
}


void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh) { return; }
    // Drive animation instance if present
    if (bUseAnimation && AnimInstance && SkeletalMesh && SkeletalMesh->GetSkeleton())
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);

        FPoseContext OutputPose;
        OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
        AnimInstance->EvaluateAnimation(OutputPose);

        // Apply local-space pose to component and rebuild skinning
        // 애니메이션 포즈를 BaseAnimationPose에 저장 (additive 적용 전 리셋용)
        BaseAnimationPose = OutputPose.LocalSpacePose;
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
        RefPose = CurrentLocalSpacePose;
        ForceRecomputePose();

        // Rebind anim instance to new skeleton
        if (AnimInstance)
        {
            AnimInstance->InitializeAnimation(this);
        }
        BuildPhysics();
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
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
            // 기존 AnimInstance를 먼저 삭제
            ObjectFactory::DeleteObject(AnimInstance);
            AnimInstance = nullptr;

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

// ==== Lua-friendly State Machine helper: switch this component to a state machine anim instance ====
void USkeletalMeshComponent::UseStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimStateMachineInstance already exists\n");
    }
}

UAnimStateMachineInstance* USkeletalMeshComponent::GetOrCreateStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    return StateMachine;
}

// ==== Lua-friendly Blend Space helper: switch this component to a blend space 2D anim instance ====
void USkeletalMeshComponent::UseBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimBlendSpaceInstance already exists\n");
    }
}

UAnimBlendSpaceInstance* USkeletalMeshComponent::GetOrCreateBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    return BS;
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

float USkeletalMeshComponent::GetAnimationPosition()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        return Single->GetPosition();
    }
    return 0.0f;
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
    // UpdateFinalSkinningMatrices()에서 UpdateSkinningMatrices()를 호출하여 본 행렬 계산 시간과 함께 전달
    UpdateFinalSkinningMatrices();
    // PerformSkinning은 CollectMeshBatches에서 전역 모드에 따라 수행됨
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

    // 본 행렬 계산 시간 측정 시작
    uint64 BoneMatrixCalcStart = FWindowsPlatformTime::Cycles64();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }

    // 본 행렬 계산 시간 측정 종료
    uint64 BoneMatrixCalcEnd = FWindowsPlatformTime::Cycles64();
    double BoneMatrixCalcTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneMatrixCalcEnd - BoneMatrixCalcStart);

    // 본 행렬 계산 시간을 부모 USkinnedMeshComponent로 전달
    // 부모에서 실제 스키닝 모드(CPU/GPU)에 따라 통계에 추가됨
    UpdateSkinningMatrices(TempFinalSkinningMatrices, BoneMatrixCalcTimeMS);
}

FBodyInstance* USkeletalMeshComponent::FindBodyInstance(const FName& BoneName)
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh || !Mesh->GetPhysicsAsset())
    {
        return nullptr;
    }
    
    UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset();
    
    int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);    
    if (BodyIndex != -1 && BodyIndex < Bodies.Num())
    {
        return Bodies[BodyIndex];
    }

    return nullptr;
}

void USkeletalMeshComponent::InitializeConstraints()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh || !Mesh->GetPhysicsAsset())
    {
        return;
    }
    
    UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset();
    
    const TArray<FConstraintSetup>& Setups = PhysicsAsset->GetContraintSetups();
    int32 SetupCount = PhysicsAsset->GetConstraintSetupCount();
    
    ClearConstraints();
    
    Constraints.Reserve(SetupCount);
    for (const FConstraintSetup& Setup : Setups)
    {
        FBodyInstance* Child = FindBodyInstance(Setup.ConstraintBone1);        
        if (!Child)
        {
            UE_LOG("[USkeletalMeshComponent/InitializeConstraints] Cant find body");
            continue;
        }
        FConstraintInstance* NewJoint = new FConstraintInstance();

        Setup.CopyToInstance(*NewJoint);

        NewJoint->ChildBody = Child;
        NewJoint->ParentBody = FindBodyInstance(Setup.ConstraintBone2);
        
        NewJoint->Initialize();

        if (NewJoint->IsValid())
        {
            Constraints.Add(NewJoint);
        }
        else
        {
            delete NewJoint;
        }
    }   
}

void USkeletalMeshComponent::PhysicsTest()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh)
    {
        return;
    }
    
    UPhysicsAsset* PhysicsAsset = GetSkeletalMesh() ? GetSkeletalMesh()->GetPhysicsAsset() : nullptr;
    if (!PhysicsAsset)
    {
        return;
    }

    FPhysScene* PhysScene = GetWorld() ? GetWorld()->GetPhysScene() : nullptr;
    if (!PhysScene)
    {
        return;
    }
    
    TArray<UBodySetup*>& Setups = PhysicsAsset->GetBodySetups();
    int32 SetupCount = Setups.Num();
    ClearBodies();
    Bodies.Reserve(SetupCount);    
    for (int32 i = 0; i < SetupCount; ++i)
    {
        UBodySetup* CurrentSetup = Setups[i];
        if (!CurrentSetup)
        {
            continue;
        }
        
        int32 BoneIndex = Mesh->GetBoneIndexFromBoneName(CurrentSetup->BoneName);        
        if (BoneIndex != -1)
        {
            FBodyInstance* Body = new FBodyInstance();
            FTransform BoneWorld = GetBoneWorldTransform(BoneIndex);
            BoneWorld.Scale3D = FVector(1.0f, 1.0f, 1.0f);
            UE_LOG("before Scene %p", PhysScene->GetPxScene());
            Body->bSimulatePhysics = false;
            Body->bEnableGravity = false;            
            Body->InitBody(CurrentSetup, BoneWorld, this, PhysScene);
            // Body->InitBody(CurrentSetup, GetWorldTransform(), this, PhysScene);
            Bodies.Add(Body);
        }        
    }
}

void USkeletalMeshComponent::ClearBodies()
{
    for (auto *Body : Bodies)
    {
        delete Body;
    }
    Bodies.Empty();
}

void USkeletalMeshComponent::ClearConstraints()
{
    for (auto *Constraint : Constraints)
    {
        delete Constraint;
    }
    Constraints.Empty();
}

void USkeletalMeshComponent::BuildPhysics()
{
    const FSkeletalMeshData* Data = GetSkeletalMesh() ? GetSkeletalMesh()->GetSkeletalMeshData() : nullptr;
    if (!Data || Data->Vertices.IsEmpty() || Data->Skeleton.Bones.IsEmpty())
    {
        return;
    }
    const FSkeleton& Skeleton = Data->Skeleton;

    UPhysicsAsset* PhysicsAsset = GetSkeletalMesh()->GetPhysicsAsset();
    if (!PhysicsAsset)
    {
        PhysicsAsset = NewObject<UPhysicsAsset>();
        GetSkeletalMesh()->SetPhysicsAsset(PhysicsAsset);
    }

    PhysicsAsset->ClearAllBodies();
    // 가중치 영향 받는 본 저장.
    TArray<FInfluencedVertex> InfluenceVertices;
    InfluenceVertices.Empty();
    InfluenceVertices.SetNum(Skeleton.Bones.Num());
    for (const FSkinnedVertex& Vertex : Data->Vertices)
    {
        for (int32 i = 0; i < 4; i++)
        {
            uint32 BoneIndex = Vertex.BoneIndices[i];
            float Weight = Vertex.BoneWeights[i];

            if (Weight >= 0.3f && BoneIndex < static_cast<uint32>(Skeleton.Bones.Num()))
            {
                InfluenceVertices[BoneIndex].Vertices.Add(Vertex.Position);
                InfluenceVertices[BoneIndex].TotalWeight += Weight;
            }
        }
    }

    for (int32 BoneIndex = 0; BoneIndex < InfluenceVertices.Num(); ++BoneIndex)
    {
        const FInfluencedVertex& Influence = InfluenceVertices[BoneIndex];
        if (Influence.Vertices.Num() > 0)
        {
            UE_LOG("  Bone %d (%s): %d influenced vertices (total weight: %.2f)",
                BoneIndex, Skeleton.Bones[BoneIndex].Name.c_str(), Influence.Vertices.Num(), Influence.TotalWeight);
        }
    }

    const TArray<FBone>& Bones = Skeleton.Bones;
    int32 CreatedCount = 0;
    for (int32 BoneIndex = 0; BoneIndex < Bones.size(); ++BoneIndex)
    {
        const FBone& Bone = Bones[BoneIndex];
        FName BoneName(Bone.Name);

        // 유효한 본 검사
        FString Name = Bone.Name;
        std::transform(Name.begin(), Name.end(), Name.begin(), ::tolower);
        if (Name.find("ik") != FString::npos ||
            Name.find("twist") != FString::npos ||
            Name.find("_end") != FString::npos ||
            Name.find("marker") != FString::npos ||
            Name.find("socket") != FString::npos ||
            Name.find("root") == 0) // Skip root bone
        {
            continue;
        }

        bool bHasChildren = false;
        for (int32 i = 0; i < Bones.size(); ++i)
        {
            if (Bones[i].ParentIndex == BoneIndex)
            {
                bHasChildren = true;
                break;
            }
        }
        if (!bHasChildren)
        {
            continue;
        }

        int32 ChildIndex = -1;
        for (int32 i = 0; i < Bones.size(); ++i)
        {
            if (Bones[i].ParentIndex == BoneIndex)
            {
                ChildIndex = i;
                break;
            }
        }

        if (ChildIndex >= 0)
        {
            FVector BonePos = FVector(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);
            FVector ChildPos = FVector(
                Bones[ChildIndex].BindPose.M[3][0],
                Bones[ChildIndex].BindPose.M[3][1],
                Bones[ChildIndex].BindPose.M[3][2]
            );

            float BoneLength = (ChildPos - BonePos).Size();

            // Exclude bones shorter than 0.01 units (1cm - excludes finger segments)
            // Mixamo models are often scaled down, so use a small threshold
            if (BoneLength < 0.05f)
            {
                UE_LOG("ShouldCreateBodyForBone: Excluding short bone '%s' (length: %.2f)", Bone.Name.c_str(), BoneLength);
                continue;
            }
        }

        if (BoneIndex >= InfluenceVertices.Num())
        {
            continue;
        }

        const FInfluencedVertex& Influence = InfluenceVertices[BoneIndex];
        if (Influence.Vertices.Num() < 3)
        {
            continue;
        }

        UBodySetup* NewBody = NewObject<UBodySetup>();
        NewBody->BoneName = BoneName;
        NewBody->BodyType = EBodySetupType::Capsule;

        FVector Centeroid = FVector::Zero();
        for (const FVector& Vertex : Influence.Vertices)
        {
            Centeroid += Vertex;
        }
        Centeroid /= static_cast<float>(Influence.Vertices.Num());

        float Cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

        for (const FVector& Vertex : Influence.Vertices)
        {
            FVector Diff = Vertex - Centeroid;
            Cov[0][0] += Diff.X * Diff.X;
            Cov[0][1] += Diff.X * Diff.Y;
            Cov[0][2] += Diff.X * Diff.Z;
            Cov[1][1] += Diff.Y * Diff.Y;
            Cov[1][2] += Diff.Y * Diff.Z;
            Cov[2][2] += Diff.Z * Diff.Z;
        }
        Cov[1][0] = Cov[0][1];
        Cov[2][0] = Cov[0][2];
        Cov[2][1] = Cov[1][2];

        FVector EigenVector = FVector(1.0f, 0.0f, 0.0f);

        for (int32 i = 0; i < 20; i++)
        {
            FVector New(
                Cov[0][0] * EigenVector.X + Cov[0][1] * EigenVector.Y + Cov[0][2] * EigenVector.Z,
                Cov[1][0] * EigenVector.X + Cov[1][1] * EigenVector.Y + Cov[1][2] * EigenVector.Z,
                Cov[2][0] * EigenVector.X + Cov[2][1] * EigenVector.Y + Cov[2][2] * EigenVector.Z
            );

            float Length = New.Size();
            if (Length > 0.0001f)
            {
                EigenVector = New / Length;
            }
            else
            {
                break;
            }
        }
        FVector PrincipalAxis = EigenVector;

        FVector FittedCenter = FVector::Zero();
        FQuat FittedRotation = FQuat::Identity();
        float Radius = 5.0f;
        float HalfHeight = 10.0f;

        for (const FVector& V : Influence.Vertices)
        {
            FittedCenter += V;
        }
        FittedCenter /= static_cast<float>(Influence.Vertices.Num());

        float MinProjection = FLT_MAX;
        float MaxProjection = -FLT_MAX;
        float MaxRadialDistance = 0.0f;

        for (const FVector& V : Influence.Vertices)
        {
            FVector Diff = V - FittedCenter;
            float AxialProjection = FVector::Dot(Diff, PrincipalAxis);

            MinProjection = FMath::Min(MinProjection, AxialProjection);
            MaxProjection = FMath::Max(MaxProjection, AxialProjection);

            // Calculate radial distance (perpendicular to axis)
            FVector AxialComponent = PrincipalAxis * AxialProjection;
            FVector RadialComponent = Diff - AxialComponent;
            float RadialDistance = RadialComponent.Size();
            MaxRadialDistance = FMath::Max(MaxRadialDistance, RadialDistance);
        }

        // Capsule dimensions
        Radius = MaxRadialDistance * 1.05f;  // Add 5% padding
        float CylinderHeight = (MaxProjection - MinProjection) * 1.05f;
        HalfHeight = CylinderHeight * 0.5f;

        // Calculate rotation from Z-axis to principal axis
        FVector DefaultDirection(0.0f, 0.0f, 1.0f);
        FVector Axis = FVector::Cross(DefaultDirection, PrincipalAxis);
        float AxisLength = Axis.Size();

        if (AxisLength > 0.0001f)
        {
            Axis /= AxisLength;
            float DotProduct = FVector::Dot(DefaultDirection, PrincipalAxis);
            float Angle = acosf(FMath::Clamp(DotProduct, -1.0f, 1.0f));

            float HalfAngle = Angle * 0.5f;
            float SinHalfAngle = sinf(HalfAngle);
            float CosHalfAngle = cosf(HalfAngle);
            FittedRotation = FQuat(Axis.X * SinHalfAngle, Axis.Y * SinHalfAngle, Axis.Z * SinHalfAngle, CosHalfAngle);
        }

        FVector LocalCenter = FVector::Zero();
        FQuat LocalRotation = FQuat::Identity();
        int32 FirstChildIndex = -1;
        for (int32 i = 0; i < Bones.Num(); ++i)
        {
            if (Bones[i].ParentIndex == BoneIndex)
            {
                FirstChildIndex = i;   // first child
                break;
            }
        }

        FTransform BoneWorldTM = GetBoneWorldTransform(BoneIndex);
        UE_LOG("Runtime Bone %d World: Pos=(%.2f,%.2f,%.2f)", BoneIndex, BoneWorldTM.Translation.X, BoneWorldTM.Translation.Y,BoneWorldTM.Translation.Z);
        BoneWorldTM.Scale3D = FVector(1, 1, 1);
        const FMatrix& BoneBind = Bones[BoneIndex].BindPose;

        FVector BoneWorldPos = BoneWorldTM.Translation;
        
        FVector ChildWorldPos = BoneWorldPos;
        if (FirstChildIndex >= 0)
        {
            FTransform ChildWorldTM = GetBoneWorldTransform(FirstChildIndex);
            ChildWorldTM.Scale3D = FVector(1, 1, 1);
            ChildWorldPos = ChildWorldTM.Translation;
        }

        FVector WorldCenter = (BoneWorldPos + ChildWorldPos) * 0.5f;
        FVector WorldOffset = WorldCenter - BoneWorldPos;

        FQuat InvRot = BoneWorldTM.Rotation.Inverse();
        LocalCenter = InvRot.RotateVector(WorldOffset);

        FVector WorldDir = (ChildWorldPos - BoneWorldPos).GetSafeNormal();
        if (!WorldDir.IsZero())
        {
            FVector LocalDir = InvRot.RotateVector(WorldDir);
            LocalRotation = FQuat::FindBetween(FVector(0, 0, 1), LocalDir);
        }

        float CapsuleLength = HalfHeight * 2.0f; // FSphylElem은 HalfHeight가 아닌 full length를 받음
        FSphylElem CapsuleElem(Radius, CapsuleLength);
        CapsuleElem.Center = LocalCenter;
        CapsuleElem.Rotation = LocalRotation;
        NewBody->AggGeom.SphylElems.Add(CapsuleElem);

        PhysicsAsset->AddBodySetup(NewBody);
        CreatedCount++;
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
}

void USkeletalMeshComponent::ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms)
{
    if (AdditiveTransforms.IsEmpty()) return;

    // CurrentLocalSpacePose는 TickComponent에서 이미 기본 애니메이션 포즈를 포함하고 있어야 함
    for (auto const& [BoneIndex, AdditiveTransform] : AdditiveTransforms)
    {
        if (BoneIndex >= 0 && BoneIndex < CurrentLocalSpacePose.Num())
        {
            // 회전이 위치에 영향을 주지 않도록 각 성분을 개별적으로 적용
            FTransform& BasePose = CurrentLocalSpacePose[BoneIndex];

            // 위치: 단순 덧셈
            FVector FinalLocation = BasePose.Translation + AdditiveTransform.Translation;

            // 회전: 쿼터니언 곱셈
            FQuat FinalRotation = AdditiveTransform.Rotation * BasePose.Rotation;

            // 스케일: 성분별 곱셈
            FVector FinalScale = FVector(
                BasePose.Scale3D.X * AdditiveTransform.Scale3D.X,
                BasePose.Scale3D.Y * AdditiveTransform.Scale3D.Y,
                BasePose.Scale3D.Z * AdditiveTransform.Scale3D.Z
            );

            CurrentLocalSpacePose[BoneIndex] = FTransform(FinalLocation, FinalRotation, FinalScale);
        }
    }

    // 모든 additive 적용 후 최종 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToRefPose()
{
    CurrentLocalSpacePose = RefPose;
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToAnimationPose()
{
    // BaseAnimationPose가 비어있으면 RefPose 사용
    if (BaseAnimationPose.IsEmpty())
    {
        CurrentLocalSpacePose = RefPose;
    }
    else
    {
        CurrentLocalSpacePose = BaseAnimationPose;
    }
    ForceRecomputePose();
}

void USkeletalMeshComponent::TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(NotifyEvent);
    }
}