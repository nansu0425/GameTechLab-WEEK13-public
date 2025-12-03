#include "pch.h"
#include "VehicleMovementComponent.h"

#include "Actor.h"
#include "GlobalConsole.h"
#include "PhysicsCore.h"
#include "PhysScene.h"
#include "PhysicsTypes.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "StaticMeshComponent.h"
#include "SkeletalMeshComponent.h"
#include "World.h"
#include "Renderer.h"
#include "Source/Runtime/Engine/Physics/Shape/SphylElem.h"

#include <PxPhysicsAPI.h>

IMPLEMENT_CLASS(UVehicleMovementComponent)

UVehicleMovementComponent::UVehicleMovementComponent()
{
    bCanEverTick = true;
    ThrottleInput = 0.0f;
    SteeringInput = 0.0f;
    BrakeInput = 0.0f;
    HandbrakeInput = 0.0f;
	ChassisHalfExtents = FVector(2.75f, 1.1f, 0.4f);

    VehicleQueryResults.wheelQueryResults = nullptr;
    VehicleQueryResults.nbWheelQueryResults = 0;

	// WheelSetups 기본값 세팅 (4륜 구동 차량)
    WheelSetups.Add({ "FL", {  1.75f, -1.2f, -0.03f }, 0.5f, true, true });
	WheelSetups.Add({ "FR", {  1.75f,  1.2f, -0.03f }, 0.5f, true, true });
	WheelSetups.Add({ "RL", { -1.75f, -1.2f, -0.03f }, 0.55f, true, false });
	WheelSetups.Add({ "RR", { -1.75f,  1.2f, -0.03f }, 0.55f, true, false });
}

UVehicleMovementComponent::~UVehicleMovementComponent() = default;

void UVehicleMovementComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
	// Physics 관련 로직은 대부분 BeginPlay에서 초기화하므로 주석처리 (문제 생기면 다시 사용)
    // SearchForUpdatedComponent();
    // RegisterWithPhysScene();
}

void UVehicleMovementComponent::OnUnregister()
{
	// Physics 관련 로직은 대부분 EndPlay에서 정리하므로 주석처리 (문제 생기면 다시 사용)
    // UnregisterFromPhysScene();
    // CleanupVehiclePhysX();
    Super::OnUnregister();
}

void UVehicleMovementComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // PIE 복제 시 UpdatedComponent가 에디터 월드 컴포넌트를 가리키는 문제를 방지.
    // 자체적으로 복제하지 않고 null로 설정하여 런타임에 다시 설정되도록 함.
    UpdatedComponent = nullptr;
    bVehicleInitialized = false;
    bRegisteredWithPhysScene = false;
}


void UVehicleMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    SearchForUpdatedComponent();
    if (UpdatedComponent)
    {
        if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent))
        {
            SkelComp->SetAnimationUsage(false); // 애니메이션 비활성화
        }
    }
	SearchForRollableWheels();
    RegisterWithPhysScene();
	InitVehiclePhysX();
}

void UVehicleMovementComponent::EndPlay()
{
    UnregisterFromPhysScene();
    CleanupVehiclePhysX();
    Super::EndPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

	TestRollWheels(DeltaTime);

    SearchForUpdatedComponent();

    if (!bVehicleInitialized)
    {
        bVehicleInitialized = InitVehiclePhysX();
        if (!bVehicleInitialized)
        {
            return;
        }
    }
}

void UVehicleMovementComponent::SetThrottleInput(float Throttle)
{
    ThrottleInput = FMath::Clamp(Throttle, -1.0f, 1.0f);
}

void UVehicleMovementComponent::SetSteeringInput(float Steering)
{
    SteeringInput = FMath::Clamp(Steering, -1.0f, 1.0f);
}

void UVehicleMovementComponent::SetBrakeInput(float Brake)
{
    BrakeInput = FMath::Clamp(Brake, 0.0f, 1.0f);
}

void UVehicleMovementComponent::SetHandbrakeInput(float Handbrake)
{
    HandbrakeInput = FMath::Clamp(Handbrake, 0.0f, 1.0f);
}

void UVehicleMovementComponent::ResetVehicle()
{
    // 기존 PhysX 리소스 정리
    CleanupVehiclePhysX();

    // UpdatedComponent 검증 및 PhysScene 등록
    SearchForUpdatedComponent();
    RegisterWithPhysScene();

    // 재초기화 시도
    bVehicleInitialized = InitVehiclePhysX();
}

void UVehicleMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    if (!NewUpdatedComponent)
    {
        UpdatedComponent = nullptr;
        return;
    }

    if (Cast<USkeletalMeshComponent>(NewUpdatedComponent) || Cast<UStaticMeshComponent>(NewUpdatedComponent))
    {
        UMovementComponent::SetUpdatedComponent(NewUpdatedComponent);
    }
    else
    {
        UE_LOG("[VehicleMovement] UpdatedComponent must be SkeletalMeshComponent or StaticMeshComponent (given: %s)",
            NewUpdatedComponent->GetClass()->Name);
        UpdatedComponent = nullptr;
    }
}

bool UVehicleMovementComponent::InitVehiclePhysX()
{
    if (bVehicleInitialized)
    {
        return true;
    }

    // =========================================================================
    // 사전 검사
	// =========================================================================


	// 검사1: UpdatedComponent 확인
    if (!UpdatedComponent)
    {
        if (!bWarnedMissingBodyComponent)
        {
            UE_LOG("[VehicleMovement] No UpdatedComponent. Set a SkeletalMeshComponent or StaticMeshComponent as UpdatedComponent.");
            bWarnedMissingBodyComponent = true;
        }
        return false;
    }

	// 검사2: WheelSetups가 완료되어 있는지 확인
    if (WheelSetups.Num() < 4)
    {
        if (!bWarnedWheelSetup)
        {
            UE_LOG("[VehicleMovement] Wheel setup array must have 4 wheels at least.");
            bWarnedWheelSetup = true;
        }
        return false;
    }

	// 검사3: PhysX가 초기화되었는지 확인
    if (!FPhysicsCore::Get().IsInitialized())
    {
        if (!bWarnedPhysicsUninitialized)
        {
            UE_LOG("[VehicleMovement] FPhysicsCore is not initialized yet.");
            bWarnedPhysicsUninitialized = true;
        }
        return false;
    }

	// 검사4: Physic Scene과 Physics Core 등 확인
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG("[VehicleMovement] World is null.");
        return false;
    }
    FPhysScene* PhysScene = World->GetPhysScene();
    if (!PhysScene || !PhysScene->IsInitialized())
    {
        UE_LOG("[VehicleMovement] PhysScene is not initialized.");
        return false;
    }
    physx::PxScene* PxScene = PhysScene->GetPxScene();
    if (!PxScene)
    {
        UE_LOG("[VehicleMovement] PxScene is null.");
        return false;
    }
    physx::PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    if (!Physics)
    {
        UE_LOG("[VehicleMovement] PxPhysics is null.");
        return false;
    }

	// =========================================================================
	// BodySetup 및 BodyInstance 생성/초기화
	// =========================================================================

    // BodySetup 생성 (BeginPlay~EndPlay 동안 유지)
    if (!VehicleBodySetup)
    {
        VehicleBodySetup = new UBodySetup();
    }
    VehicleBodySetup->AggGeom.EmptyElements();

    // 차체 박스 shape
    {
        FBoxElem BoxElem;
        BoxElem.X = FMath::Max(ChassisHalfExtents.X * 2.0f, KINDA_SMALL_NUMBER);
        BoxElem.Y = FMath::Max(ChassisHalfExtents.Y * 2.0f, KINDA_SMALL_NUMBER);
        BoxElem.Z = FMath::Max(ChassisHalfExtents.Z * 2.0f, KINDA_SMALL_NUMBER);
        BoxElem.Center = ChassisOffset;
        BoxElem.Rotation = FQuat::Identity();
        VehicleBodySetup->AggGeom.BoxElems.Add(BoxElem);
    }

    // 휠 shape (캡슐로 근사, 얇은 두께)
    const float WheelThickness = 0.1f; // 고정 두께
    const FQuat WheelRot = FQuat::MakeFromEulerZYX(FVector(0.f, 0.f, -90.f)); // 축을 X 방향으로
    for (const FWheelSetup& Setup : WheelSetups)
    {
        FSphylElem Sphyl;
        Sphyl.Radius = FMath::Max(Setup.WheelRadius, 0.0f);
        Sphyl.Length = WheelThickness;
        Sphyl.Center = Setup.DefaultPosition + ChassisOffset;
        Sphyl.Rotation = WheelRot;
        VehicleBodySetup->AggGeom.SphylElems.Add(Sphyl);
    }

    // BodyInstance 생성/초기화
    UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!PrimComp)
    {
        UE_LOG("[VehicleMovement] UpdatedComponent is not a primitive component.");
        return false;
    }

    VehicleBodyInstance.bSimulatePhysics = true;
    VehicleBodyInstance.MassInKg = VehicleMass;

    FTransform BodyTransform = UpdatedComponent ? UpdatedComponent->GetWorldTransform() : FTransform();
    VehicleBodyInstance.InitBody(VehicleBodySetup, BodyTransform, PrimComp, PhysScene);

    if (!VehicleBodyInstance.IsInitialized())
    {
        UE_LOG("[VehicleMovement] Failed to initialize BodyInstance.");
        return false;
    }
    // =========================================================================
    // PhysX 리소스 생성/초기화
    // =========================================================================

    // 휠 쿼리 버퍼 초기화
    WheelQueryResults.SetNum(WheelSetups.Num());
    VehicleQueryResults.wheelQueryResults = WheelQueryResults.GetData();
    VehicleQueryResults.nbWheelQueryResults = WheelQueryResults.Num();

    // 마찰 테이블 준비 (노면 1, 타이어 1)
    if (!TireFrictionPairs)
    {
        physx::PxMaterial* DefaultMat = PhysScene->GetDefaultMaterial();
        if (!DefaultMat)
        {
            DefaultMat = FPhysicsCore::Get().CreateMaterial(0.5f, 0.5f, 0.6f);
        }

        TireFrictionPairs = physx::PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);
        physx::PxVehicleDrivableSurfaceType SurfaceTypes[1];
        SurfaceTypes[0].mType = 0;
        const physx::PxMaterial* SurfaceMats[1] = { DefaultMat };
        TireFrictionPairs->setup(1, 1, SurfaceMats, SurfaceTypes);
        TireFrictionPairs->setTypePairFriction(0, 0, 1.0f); // Tire Type 0 & Surface Type 0 => Friction 1.0f
    }

    // Drive4W 생성
    const physx::PxU32 NumWheels = WheelSetups.Num();

    // 스프링 질량 계산용 오프셋
    TArray<physx::PxVec3> PxSprungMassOffsets;
    PxSprungMassOffsets.SetNum(NumWheels);
    for (physx::PxU32 i = 0; i < NumWheels; ++i)
    {
        FVector WheelCentreLocal = WheelSetups[i].DefaultPosition + ChassisOffset;
        WheelCentreLocal.Z -= WheelSetups[i].WheelRadius;
        PxSprungMassOffsets[i] = PhysicsConversion::ToPxVec3(WheelCentreLocal);
    }

    TArray<float> SprungMasses;
    SprungMasses.SetNum(NumWheels);
    physx::PxVehicleComputeSprungMasses(
        NumWheels,
        PxSprungMassOffsets.GetData(),
        physx::PxVec3(0.0f, 0.0f, 0.0f),
        VehicleMass,
        1,
        SprungMasses.GetData());

    // WheelsSimData 구성
    physx::PxVehicleWheelsSimData* WheelsSimData = physx::PxVehicleWheelsSimData::allocate(NumWheels);

    const float WheelWidth = 0.4f;
    const float WheelMass = 20.0f;
    const float SuspensionTravel = 0.3f;

    for (physx::PxU32 i = 0; i < NumWheels; ++i)
    {
        const FWheelSetup& Setup = WheelSetups[i];

        physx::PxVehicleWheelData WheelData;
        WheelData.mRadius = Setup.WheelRadius;
        WheelData.mWidth = WheelWidth;
        WheelData.mMass = WheelMass;
        WheelData.mMOI = 0.5f * WheelMass * Setup.WheelRadius * Setup.WheelRadius;
        WheelData.mMaxHandBrakeTorque = Setup.bIsDriveWheel ? MaxHandbrakeTorque : 0.0f;
        WheelData.mMaxBrakeTorque = MaxBrakeTorque;
        WheelData.mMaxSteer = physx::PxPi * MaxSteerAngle / 180.0f;

        physx::PxVehicleSuspensionData SusData;
        SusData.mMaxCompression = SuspensionTravel;
        SusData.mMaxDroop = SuspensionTravel;
        SusData.mSpringStrength = 35000.0f;
        SusData.mSpringDamperRate = 4500.0f;
        SusData.mSprungMass = (i < (physx::PxU32)SprungMasses.Num()) ? SprungMasses[i] : VehicleMass / NumWheels;

        FVector WheelCentreLocal = Setup.DefaultPosition + ChassisOffset;
        WheelCentreLocal.Z -= Setup.WheelRadius;
        physx::PxVec3 WheelCentreOffset = PhysicsConversion::ToPxVec3(WheelCentreLocal);

        WheelsSimData->setWheelData(i, WheelData);
        WheelsSimData->setSuspensionData(i, SusData);
        WheelsSimData->setWheelCentreOffset(i, WheelCentreOffset);
        WheelsSimData->setSuspTravelDirection(i, physx::PxVec3(0.0f, -1.0f, 0.0f));
        WheelsSimData->setSuspForceAppPointOffset(i, WheelCentreOffset);
        WheelsSimData->setTireForceAppPointOffset(i, WheelCentreOffset);
    }

    // 휠 shape 매핑 (BodySetup 생성 순서: 박스 1개 + 휠 캡슐 N개)
    {
        const physx::PxU32 WheelShapeStart = 1; // 박스 이후 첫 캡슐 인덱스
        for (physx::PxU32 i = 0; i < NumWheels; ++i)
        {
            WheelsSimData->setWheelShapeMapping(i, WheelShapeStart + i);
        }
    }

    // DriveSimData 구성
    physx::PxVehicleDriveSimData4W DriveSimData;
    physx::PxVehicleEngineData EngineData;
    EngineData.mPeakTorque = MaxDriveTorque;
    EngineData.mMaxOmega = 600.0f;
    DriveSimData.setEngineData(EngineData);

    physx::PxVehicleGearsData GearsData;
    DriveSimData.setGearsData(GearsData);

    physx::PxVehicleClutchData ClutchData;
    DriveSimData.setClutchData(ClutchData);

    physx::PxVehicleAckermannGeometryData Ackermann;
    Ackermann.mFrontWidth = 150.0f;
    Ackermann.mRearWidth = 150.0f;
    Ackermann.mAxleSeparation = 250.0f;
    DriveSimData.setAckermannGeometryData(Ackermann);

    physx::PxVehicleSetBasisVectors(physx::PxVec3(0.0f, 1.0f, 0.0f), physx::PxVec3(0.0f, 0.0f, -1.0f));
    physx::PxVehicleSetUpdateMode(physx::PxVehicleUpdateMode::eVELOCITY_CHANGE);

    physx::PxRigidDynamic* ChassisActor = VehicleBodyInstance.GetPxRigidDynamic();
    if (!ChassisActor)
    {
        UE_LOG("[VehicleMovement] BodyInstance has no PxRigidDynamic.");
        WheelsSimData->free();
        return false;
    }

    physx::PxVehicleDrive4W* Drive4W = physx::PxVehicleDrive4W::allocate(NumWheels);
    Drive4W->setup(Physics, ChassisActor, *WheelsSimData, DriveSimData, NumWheels);
    WheelsSimData->free();

    if (!Drive4W->getRigidDynamicActor())
    {
        Drive4W->free();
        UE_LOG("[VehicleMovement] PxVehicleDrive4W setup failed (no actor bound).");
        return false;
    }

    PxVehicleDrive4WInstance = Drive4W;

    bVehicleInitialized = true;
    return true;
}

void UVehicleMovementComponent::ApplyInputToPhysX(float DeltaTime)
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    physx::PxVehicleDrive4WRawInputData RawInput;
    const bool bAccel = ThrottleInput > 0.1f;
    const bool bBrake = BrakeInput > 0.1f;
    const bool bHandbrake = HandbrakeInput > 0.1f;
    const bool bSteerLeft = SteeringInput < -0.1f;
    const bool bSteerRight = SteeringInput > 0.1f;

    RawInput.setDigitalAccel(bAccel);
    RawInput.setDigitalBrake(bBrake);
    RawInput.setDigitalHandbrake(bHandbrake);
    RawInput.setDigitalSteerLeft(bSteerLeft);
    RawInput.setDigitalSteerRight(bSteerRight);
    RawInput.setGearUp(false);
    RawInput.setGearDown(false);

    // 기본 스무딩 데이터 (키보드 입력용 - PhysX 샘플 기본값과 유사)
    static const physx::PxVehicleKeySmoothingData KeySmoothing = {
        {6.0f, 6.0f, 6.0f, 6.0f},
        {10.0f, 10.0f, 10.0f, 10.0f}
    };

    // 스티어링 감쇠 테이블 (속도가 빨라질수록 최대 스티어 각을 줄임)
    static const physx::PxReal SteerVsSpeedData[] = {
        0.0f,   1.0f,
        15.0f,  0.7f,
        30.0f,  0.5f,
        120.0f, 0.3f
    };
    static const physx::PxFixedSizeLookupTable<8> SteerVsForwardSpeedTable(SteerVsSpeedData, 4);

    const float SmoothedDeltaTime = FMath::Max(DeltaTime, 1e-3f);

    bool bVehicleInAir = true;
    for (const physx::PxWheelQueryResult& Result : WheelQueryResults)
    {
        if (!Result.isInAir)
        {
            bVehicleInAir = false;
            break;
        }
    }

    physx::PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(
        KeySmoothing,
        SteerVsForwardSpeedTable,
        RawInput,
        SmoothedDeltaTime,
        bVehicleInAir,
        *PxVehicleDrive4WInstance);
}

void UVehicleMovementComponent::PerformSuspensionRaycasts()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    if (!UpdatedComponent)
    {
        return;
    }

    UWorld* World = UpdatedComponent->GetWorld();
    if (!World)
    {
        return;
    }

    FPhysScene* PhysScene = World->GetPhysScene();
    if (!PhysScene || !PhysScene->IsInitialized())
    {
        return;
    }

    physx::PxScene* PxScene = PhysScene->GetPxScene();
    if (!PxScene)
    {
        return;
    }

    // WheelQueryResults 크기 보정
    if (WheelQueryResults.Num() != WheelSetups.Num())
    {
        WheelQueryResults.SetNum(WheelSetups.Num());
        VehicleQueryResults.wheelQueryResults = WheelQueryResults.GetData();
        VehicleQueryResults.nbWheelQueryResults = WheelQueryResults.Num();
    }

    // 차체 월드 트랜스폼
    const FTransform ChassisWorld = UpdatedComponent->GetWorldTransform();

    // 레이캐스트 설정
    const physx::PxHitFlags HitFlags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL;

    for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
    {
        physx::PxWheelQueryResult& Result = WheelQueryResults[WheelIdx];
        const FWheelSetup& Setup = WheelSetups[WheelIdx];

        // 기본값: 공중 상태로 초기화
        Result = physx::PxWheelQueryResult();
        Result.isInAir = true;

        // 휠 시작점(차체 로컬 오프셋을 다시 계산)
        FVector WheelCentreLocal = FVector::Zero();
        if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent))
        {
            USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
            const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData() ? &SkelMesh->GetSkeletalMeshData()->Skeleton : nullptr : nullptr;
            if (Skeleton)
            {
                int32 BoneIndex = -1;
                for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
                {
                    if (Skeleton->Bones[i].Name == Setup.BoneName)
                    {
                        BoneIndex = i;
                        break;
                    }
                }

                if (BoneIndex != -1)
                {
                    FTransform BoneWorld = SkelComp->GetBoneWorldTransform(BoneIndex);
                    FTransform BoneLocalToChassis = BoneWorld.GetRelativeTransform(ChassisWorld);
                    WheelCentreLocal = BoneLocalToChassis.Translation;
                }
            }
        }

        WheelCentreLocal.Z -= Setup.WheelRadius;

        const FVector StartMundi = ChassisWorld.TransformPosition(WheelCentreLocal);
        const FVector DirMundi = FVector(0.0f, 0.0f, -1.0f); // 다운 방향

        const float TraceLength = FMath::Max(Setup.WheelRadius + 50.0f, 50.0f);

        const physx::PxVec3 Origin = PhysicsConversion::ToPxVec3(StartMundi);
        const physx::PxVec3 Direction = PhysicsConversion::ToPxVec3(DirMundi).getNormalized();

        physx::PxRaycastBuffer RaycastHit;
        bool bHit = PxScene->raycast(Origin, Direction, TraceLength, RaycastHit, HitFlags);

        if (bHit && RaycastHit.hasBlock)
        {
            Result.isInAir = false;
            Result.tireContactPoint = RaycastHit.block.position;
            Result.tireContactNormal = RaycastHit.block.normal;
            Result.tireContactShape = RaycastHit.block.shape;
            Result.tireContactActor = RaycastHit.block.actor;
            Result.tireSurfaceMaterial = RaycastHit.block.shape ? RaycastHit.block.shape->getMaterialFromInternalFaceIndex(RaycastHit.block.faceIndex) : nullptr;
            Result.tireSurfaceType = 0;
            Result.suspJounce = TraceLength - RaycastHit.block.distance;
        }
        else
        {
            Result.isInAir = true;
        }
    }
}

void UVehicleMovementComponent::SimulateVehicle(float DeltaTime)
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    // 현재 중력 가져오기
    FVector GravityMundi = FVector(0, 0, -981.0f);
    if (UpdatedComponent)
    {
        if (UWorld* World = UpdatedComponent->GetWorld())
        {
            if (FPhysScene* PhysScene = World->GetPhysScene())
            {
                GravityMundi = PhysScene->GetGravity();
            }
        }
    }

    const physx::PxVec3 GravityPx = PhysicsConversion::ToPxVec3(GravityMundi);

    // 고정 스텝 기반 시뮬레이션 (PhysSceneImpl의 설정에 맞춰 1/60 사용)
    const float FixedTimestep = 1.0f / 60.0f;

    physx::PxVehicleWheelQueryResult* QueryResults = &VehicleQueryResults;
    physx::PxVehicleWheels* Vehicles[1] = { PxVehicleDrive4WInstance };

    physx::PxVehicleUpdates(FixedTimestep, GravityPx, *TireFrictionPairs, 1, Vehicles, QueryResults);
}

void UVehicleMovementComponent::UpdateVehiclePoseFromPhysX()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance || !UpdatedComponent)
    {
        return;
    }

    physx::PxRigidDynamic* ChassisActor = PxVehicleDrive4WInstance ? PxVehicleDrive4WInstance->getRigidDynamicActor() : nullptr;
    if (!ChassisActor)
    {
        return;
    }

    // PhysX 차체 글로벌 포즈를 엔진 Transform으로 변환하여 UpdatedComponent에 적용
    physx::PxTransform PxPose = ChassisActor->getGlobalPose();
    FTransform WorldTransform = PhysicsConversion::ToFTransform(PxPose);

    // UpdatedComponent가 루트가 아닌 경우 경고(60프레임마다 한 번)
    bool bIsRootComponent = false;
    if (AActor* OwnerActor = Cast<AActor>(Owner))
    {
        if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
        {
            bIsRootComponent = (RootComp == UpdatedComponent);
        }
    }
    if (!bIsRootComponent)
    {
        NonRootComponentWarningCounter++;
        if (NonRootComponentWarningCounter >= 60)
        {
            UE_LOG("[VehicleMovement] UpdatedComponent is not the root component; Actor/child components may desync.");
            NonRootComponentWarningCounter = 0;
        }
    }
    else
    {
        NonRootComponentWarningCounter = 0;
    }

    // UpdatedComponent는 차체 메시(스켈레탈/스태틱)여야 하므로 World Transform 설정
    UpdatedComponent->SetWorldTransform(WorldTransform);

    // 휠 본 승강 반영 (스켈레탈 메시에만 적용, 회전/스티어는 추후)
    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent))
    {
        USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
        const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData() ? &SkelMesh->GetSkeletalMeshData()->Skeleton : nullptr : nullptr;
        if (Skeleton)
        {
            // WheelQueryResults와 캐싱된 로컬 위치를 안전하게 접근
            const int32 NumWheels = WheelSetups.Num();
            for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
            {
                const FWheelSetup& Setup = WheelSetups[WheelIdx];

                // 본 인덱스 찾기
                int32 BoneIndex = -1;
                for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
                {
                    if (Skeleton->Bones[i].Name == Setup.BoneName)
                    {
                        BoneIndex = i;
                        break;
                    }
                }

                if (BoneIndex == -1)
                {
                    continue;
                }

                // 현재 로컬 트랜스폼에서 회전/스케일 유지, 위치만 수정
                FTransform Local = SkelComp->GetBoneLocalTransform(BoneIndex);

                FVector BaseLocalPos = (WheelIdx < InitialWheelLocalPositions.Num())
                    ? InitialWheelLocalPositions[WheelIdx]
                    : Local.Translation;

                float SuspJounce = (WheelIdx < WheelQueryResults.Num()) ? WheelQueryResults[WheelIdx].suspJounce : 0.0f;
                bool bInAir = (WheelIdx < WheelQueryResults.Num()) ? WheelQueryResults[WheelIdx].isInAir : true;

                // 접지 시: 압축만큼 위로 이동, 공중 시: 기본 위치 유지
                FVector NewLocalPos = BaseLocalPos;
                if (!bInAir)
                {
                    NewLocalPos.Z -= SuspJounce;
                }

                Local.Translation = NewLocalPos;

                SkelComp->SetBoneLocalTransform(BoneIndex, Local);
            }
        }
    }
}

/**
 * @brief 유효한 UpdatedComponent 찾아 저장합니다.
 * @note SkeletalMeshComponent 또는 StaticMeshComponent만을 유효한 컴포넌트로 취급합니다.
 * UpdatedComponent -> 루트 컴포넌트 -> Actor의 SkeletalMeshComponent -> StaticMeshComponent 순으로 탐색합니다.
 */
void UVehicleMovementComponent::SearchForUpdatedComponent()
{
    // UpdatedComponent가 올바른 타입/Owner이면 그대로 사용
    if (UpdatedComponent && UpdatedComponent->GetOwner() == Owner)
    {
        if (Cast<USkeletalMeshComponent>(UpdatedComponent) || Cast<UStaticMeshComponent>(UpdatedComponent))
        {
            return;
        }
        // 타입이 맞지 않으면 해제
        UpdatedComponent = nullptr;
    }

    if (!Owner)
    {
        return;
    }

    if (AActor* OwnerActor = Cast<AActor>(Owner))
    {
        // 1) 루트가 메시 타입이면 우선 적용
        if (!UpdatedComponent)
        {
            if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
            {
                if (Cast<USkeletalMeshComponent>(RootComp) || Cast<UStaticMeshComponent>(RootComp))
                {
                    UE_LOG("루트 컴포넌트 발견");
                    SetUpdatedComponent(RootComp);
                }
            }
        }

        // 2) 스켈레탈 메시 탐색
        if (!UpdatedComponent)
        {
            if (UActorComponent* FoundComp = OwnerActor->GetComponent(USkeletalMeshComponent::StaticClass()))
            {
                SetUpdatedComponent(Cast<USceneComponent>(FoundComp));
            }
        }

        // 3) 스태틱 메시 탐색
        if (!UpdatedComponent)
        {
            if (UActorComponent* FoundComp = OwnerActor->GetComponent(UStaticMeshComponent::StaticClass()))
            {
                SetUpdatedComponent(Cast<USceneComponent>(FoundComp));
            }
        }
    }

    if (!UpdatedComponent && !bWarnedMissingBodyComponent)
    {
        UE_LOG("[VehicleMovement] Body component not found. Assign a SkeletalMeshComponent or StaticMeshComponent via SetUpdatedComponent.");
        bWarnedMissingBodyComponent = true;
    }
}

/**
 * @brief 바퀴 본 이름으로부터 움직일 수 있는 바퀴 본 인덱스를 찾아 캐싱합니다.
 * @note UpdatedComponent가 SkeletalMeshComponent인 경우에만 동작합니다.
 * 모든 바퀴가 유효한 본을 찾은 경우에만 본 인덱스를 캐싱하고 bUseRollableWheels가 true로 설정됩니다.
 */
void UVehicleMovementComponent::SearchForRollableWheels()
{
    bUseRollableWheels = false;
    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent);
    if (!SkelComp) return;

    USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
    const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData() ? &SkelMesh->GetSkeletalMeshData()->Skeleton : nullptr : nullptr;
	if (!Skeleton) return;

    for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
    {
        FWheelSetup& Setup = WheelSetups[WheelIdx];
        for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
        {
            if (Skeleton->Bones[i].Name == Setup.BoneName)
            {
                Setup.BoneIndex = i;
                break;
            }
        }

        if (Setup.BoneIndex == -1)
        {
            if (!bWarnedMissingWheelBone)
            {
                UE_LOG("[VehicleMovement] Wheel bone not found: %s", Setup.BoneName.ToString().c_str());
                bWarnedMissingWheelBone = true;
            }
            return;
        }
    }
	bUseRollableWheels = true;
}

void UVehicleMovementComponent::CleanupVehiclePhysX()
{
    if (PxVehicleDrive4WInstance)
    {
        PxVehicleDrive4WInstance->free();
        PxVehicleDrive4WInstance = nullptr;
    }

    if (TireFrictionPairs)
    {
        TireFrictionPairs->release();
        TireFrictionPairs = nullptr;
    }

    // BodyInstance/BodySetup 정리
    VehicleBodyInstance.TermBody();
    if (VehicleBodySetup)
    {
        delete VehicleBodySetup;
        VehicleBodySetup = nullptr;
    }

    WheelQueryResults.clear();
    VehicleQueryResults.wheelQueryResults = nullptr;
    VehicleQueryResults.nbWheelQueryResults = 0;

    bVehicleInitialized = false;
    bWarnedMissingBodyComponent = false;
    bWarnedPhysicsUninitialized = false;
    bWarnedWheelSetup = false;
}

/** PhysScene 레지스트리에 등록 */
void UVehicleMovementComponent::RegisterWithPhysScene()
{
    if (bRegisteredWithPhysScene) return;

    UWorld* World = GetWorld();
    if (!World)
    {
		UE_LOG("[VehicleMovement] Failed to register vehicle component: World is null.");
        return;
    }

    FPhysScene* PhysScene = World->GetPhysScene();
    if (!PhysScene)
    {
        UE_LOG("[VehicleMovement] Failed to register vehicle component: PhysScene is null.");
        return;  
    }
    
    PhysScene->RegisterVehicleComponent(this);
    bRegisteredWithPhysScene = true;
}

/** PhysScene 레지스트리에서 해제 */
void UVehicleMovementComponent::UnregisterFromPhysScene()
{
    if (!bRegisteredWithPhysScene) return;
    
    if (UWorld* World = GetWorld())
    {
        if (FPhysScene* PhysScene = World->GetPhysScene())
        {
            PhysScene->UnregisterVehicleComponent(this);
        }
    }

    bRegisteredWithPhysScene = false;
}

void UVehicleMovementComponent::RenderDebugLines(URenderer* Renderer)
{
    if (!Renderer)
    {
        return;
    }

    // 루트 기준 로컬 → 월드 변환
    FTransform RootTransform;
    if (AActor* OwnerActor = Cast<AActor>(Owner))
    {
        if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
        {
            RootTransform = RootComp->GetWorldTransform();
        }
    }

    auto ToWorld = [&RootTransform](const FVector& Local) -> FVector
    {
        return RootTransform.TransformPosition(Local);
    };

    // ── 차체 박스 (half extent)
    FVector Extents = ChassisHalfExtents;
    Extents.X = FMath::Abs(Extents.X);
    Extents.Y = FMath::Abs(Extents.Y);
    Extents.Z = FMath::Abs(Extents.Z);

    const FVector ChassisCenter = ChassisOffset;

    const FVector Corners[8] = {
        FVector(-Extents.X, -Extents.Y, -Extents.Z) + ChassisCenter,
        FVector(-Extents.X, -Extents.Y,  Extents.Z) + ChassisCenter,
        FVector(-Extents.X,  Extents.Y, -Extents.Z) + ChassisCenter,
        FVector(-Extents.X,  Extents.Y,  Extents.Z) + ChassisCenter,
        FVector( Extents.X, -Extents.Y, -Extents.Z) + ChassisCenter,
        FVector( Extents.X, -Extents.Y,  Extents.Z) + ChassisCenter,
        FVector( Extents.X,  Extents.Y, -Extents.Z) + ChassisCenter,
        FVector( Extents.X,  Extents.Y,  Extents.Z) + ChassisCenter,
    };

    const int32 BoxEdges[12][2] = {
        {0,1}, {0,2}, {0,4},
        {7,6}, {7,5}, {7,3},
        {1,3}, {1,5},
        {2,3}, {2,6},
        {4,5}, {4,6},
    };

    const FVector4 ChassisColor(0.0f, 1.0f, 0.0f, 1.0f);
    for (const auto& Edge : BoxEdges)
    {
        Renderer->AddLine(ToWorld(Corners[Edge[0]]), ToWorld(Corners[Edge[1]]), ChassisColor);
    }

    // ── 휠 (로컬 기본 위치 + 반지름, 얇은 두께 0.1)
    const float HalfThickness = 0.05f;
    const int32 Segments = 24;
    const float Step = 2.0f * PI / static_cast<float>(Segments);
    const FVector4 WheelColor(0.0f, 0.6f, 1.0f, 1.0f);

    for (const FWheelSetup& Setup : WheelSetups)
    {
        const float Radius = FMath::Max(Setup.WheelRadius, 0.0f);
        if (Radius <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        FVector Center = Setup.DefaultPosition + ChassisOffset;

        // 두 개의 얇은 링을 그리고, 대응 점을 연결해 얇은 두께를 표현
        TArray<FVector> RingA;
        TArray<FVector> RingB;
        RingA.Reserve(Segments);
        RingB.Reserve(Segments);

        for (int32 i = 0; i < Segments; ++i)
        {
            const float Angle = Step * static_cast<float>(i);
            const float X = Radius * std::cosf(Angle);
            const float Z = Radius * std::sinf(Angle);

            RingA.Add(Center + FVector(X, -HalfThickness, Z));
            RingB.Add(Center + FVector(X,  HalfThickness, Z));
        }

        // 링 자체
        for (int32 i = 0; i < Segments; ++i)
        {
            const int32 Next = (i + 1) % Segments;
            Renderer->AddLine(ToWorld(RingA[i]), ToWorld(RingA[Next]), WheelColor);
            Renderer->AddLine(ToWorld(RingB[i]), ToWorld(RingB[Next]), WheelColor);
        }

        // 앞/뒤 연결선 (두께 표현)
        for (int32 i = 0; i < Segments; ++i)
        {
            Renderer->AddLine(ToWorld(RingA[i]), ToWorld(RingB[i]), WheelColor);
        }
    }
}

// ============================================================================================
// 아래는 테스트 전용 코드 영역
// ============================================================================================
#pragma region TestCode

void UVehicleMovementComponent::TestRollWheels(float DeltaTime)
{
    if (!bTestMode)
        return;

    USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent);
    if (!SkelComp)
        return;

    const float WheelAngleDelta = DeltaTime * 4.0f; // 증분 각도(rad) 원하는 속도로 조정

    USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
    const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData()
        ? &SkelMesh->GetSkeletalMeshData()->Skeleton
        : nullptr
        : nullptr;

    static bool bLoggedBoneNames = false;
    static bool bLoggedWheelSetups = false;

    if (Skeleton && !bLoggedBoneNames)
    {
        UE_LOG("[VehicleMovement] SkeletalMesh Bones (%d):", static_cast<int32>(Skeleton->Bones.Num()));
        for (int32 BoneIdx = 0; BoneIdx < Skeleton->Bones.Num(); ++BoneIdx)
        {
            const FBone& Bone = Skeleton->Bones[BoneIdx];
            UE_LOG("  [%d] %s", BoneIdx, Bone.Name.c_str());
        }
        bLoggedBoneNames = true;
    }

    if (!bLoggedWheelSetups)
    {
        UE_LOG("[VehicleMovement] WheelSetups (%d):", WheelSetups.Num());
        for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
        {
            const FWheelSetup& Setup = WheelSetups[WheelIdx];
            UE_LOG("  [%d] Bone=%s Radius=%.2f Drive=%d",
                WheelIdx,
                Setup.BoneName.ToString().c_str(),
                Setup.WheelRadius,
                Setup.bIsDriveWheel ? 1 : 0);
        }
        bLoggedWheelSetups = true;
    }

    if (Skeleton)
    {
        for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
        {
            const FWheelSetup& Setup = WheelSetups[WheelIdx];
            int32 BoneIndex = -1;
            for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
            {
                if (Skeleton->Bones[i].Name == Setup.BoneName)
                {
                    BoneIndex = i;
                    break;
                }
            }

            if (BoneIndex != -1)
            {
                FTransform Local = SkelComp->GetBoneLocalTransform(BoneIndex);
                FQuat RollDelta = FQuat::MakeFromEulerZYX(FVector(WheelAngleDelta * 180.f / PI, 0.f, 0.f)); // X축 롤 증분
                Local.Rotation = RollDelta * Local.Rotation; // 증분 회전을 누적
                SkelComp->SetBoneLocalTransform(BoneIndex, Local);
            }
        }
    }
    else
    {
        UE_LOG("[VehicleMovement] SkeletalMesh or Skeleton is null in TickComponent.");
    }
}
#pragma endregion
