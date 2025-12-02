#include "pch.h"
#include "SimpleWheeledVehicleMovementComponent.h"

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

#include <PxPhysicsAPI.h>

IMPLEMENT_CLASS(USimpleWheeledVehicleMovementComponent)

USimpleWheeledVehicleMovementComponent::USimpleWheeledVehicleMovementComponent()
{
    bCanEverTick = true;
    ThrottleInput = 0.0f;
    SteeringInput = 0.0f;
    BrakeInput = 0.0f;
    HandbrakeInput = 0.0f;

    VehicleQueryResults.wheelQueryResults = nullptr;
    VehicleQueryResults.nbWheelQueryResults = 0;
}

USimpleWheeledVehicleMovementComponent::~USimpleWheeledVehicleMovementComponent() = default;

void USimpleWheeledVehicleMovementComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    EnsureUpdatedComponentIsValid();
    RegisterWithPhysScene();
}

void USimpleWheeledVehicleMovementComponent::OnUnregister()
{
    UnregisterFromPhysScene();
    CleanupVehiclePhysX();
    Super::OnUnregister();
}

void USimpleWheeledVehicleMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!bRegisteredWithPhysScene)
    {
        RegisterWithPhysScene();
	}
}

void USimpleWheeledVehicleMovementComponent::EndPlay()
{
    UnregisterFromPhysScene();
    CleanupVehiclePhysX();
    Super::EndPlay();
}

void USimpleWheeledVehicleMovementComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    EnsureUpdatedComponentIsValid();

    if (!bVehicleInitialized)
    {
        bVehicleInitialized = InitVehiclePhysX();
        if (!bVehicleInitialized)
        {
            return;
        }
    }
}

void USimpleWheeledVehicleMovementComponent::SetThrottleInput(float Throttle)
{
    ThrottleInput = FMath::Clamp(Throttle, -1.0f, 1.0f);
}

void USimpleWheeledVehicleMovementComponent::SetSteeringInput(float Steering)
{
    SteeringInput = FMath::Clamp(Steering, -1.0f, 1.0f);
}

void USimpleWheeledVehicleMovementComponent::SetBrakeInput(float Brake)
{
    BrakeInput = FMath::Clamp(Brake, 0.0f, 1.0f);
}

void USimpleWheeledVehicleMovementComponent::SetHandbrakeInput(float Handbrake)
{
    HandbrakeInput = FMath::Clamp(Handbrake, 0.0f, 1.0f);
}

void USimpleWheeledVehicleMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
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

bool USimpleWheeledVehicleMovementComponent::InitVehiclePhysX()
{
    if (bVehicleInitialized)
    {
        return true;
    }

    EnsureUpdatedComponentIsValid();

	// 검사1: UpdatedComponent가 UPrimitiveComponent인지 확인
    UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(UpdatedComponent);
    if (!PrimComp)
    {
        if (!bWarnedMissingBodyComponent)
        {
            UE_LOG("[VehicleMovement] UpdatedComponent is not a primitive component. Set a SkeletalMeshComponent or StaticMeshComponent as UpdatedComponent.");
            bWarnedMissingBodyComponent = true;
        }
        return false;
    }

	// 검사2: WheelSetups가 비어있지는 않은지 확인
    if (WheelSetups.Num() == 0)
    {
        if (!bWarnedWheelSetup)
        {
            UE_LOG("[VehicleMovement] Wheel setup array is empty. Add at least one wheel before initializing.");
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

    UWorld* World = PrimComp->GetWorld();
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

    // BodyInstance가 생성한 차체 Dynamic Actor 재사용
    physx::PxRigidDynamic* ChassisActor;
    if(USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
    {
		TArray<FBodyInstance*> Bodies = SkelComp->GetBodies();
		ChassisActor = Bodies[0]->GetPxRigidDynamic();
    }
    else
    {
        ChassisActor = PrimComp->GetBodyInstanceRef().GetPxRigidDynamic();
	}
    if (!ChassisActor)
    {
        UE_LOG("[VehicleMovement] BodyInstance does not have a valid PxRigidDynamic. Ensure CreatePhysicsState() was called.");
        return false;
    }

    // 휠 위치를 차체 로컬 기준(Mundi)으로 계산 (스켈레탈 본 기반)
    TArray<FVector> WheelCentreOffsets;
    WheelCentreOffsets.SetNum(WheelSetups.Num());

    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(PrimComp))
    {
        USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
        const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData() ? &SkelMesh->GetSkeletalMeshData()->Skeleton : nullptr : nullptr;
        if (Skeleton)
        {
            const FTransform ChassisWorld = PrimComp->GetWorldTransform();
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

                if (BoneIndex == -1)
                {
                    if (!bWarnedMissingWheelBone)
                    {
                        UE_LOG("[VehicleMovement] Wheel bone not found: %s", Setup.BoneName.ToString().c_str());
                        bWarnedMissingWheelBone = true;
                    }
                    WheelCentreOffsets[WheelIdx] = FVector::Zero();
                    continue;
                }

                FTransform BoneWorld = SkelComp->GetBoneWorldTransform(BoneIndex);
                FTransform BoneLocalToChassis = BoneWorld.GetRelativeTransform(ChassisWorld);
                WheelCentreOffsets[WheelIdx] = BoneLocalToChassis.Translation;
            }
        }
    }

    const physx::PxU32 NumWheels = WheelSetups.Num();
    physx::PxVehicleWheelsSimData* WheelsSimData = physx::PxVehicleWheelsSimData::allocate(NumWheels);

    // 휠 쿼리 버퍼 준비
    WheelQueryResults.SetNum(NumWheels);
    VehicleQueryResults.wheelQueryResults = WheelQueryResults.GetData();
    VehicleQueryResults.nbWheelQueryResults = NumWheels;

    // 기본 마찰 테이블 준비 (노면 1, 타이어 1)
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

    // 기본 설정
    const float WheelWidth = 0.4f;
    const float WheelMass = 20.0f;
    const float SuspensionTravel = 0.3f;

    physx::PxVec3 WheelCentre(0.0f);

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

        FVector WheelCentreLocal = (i < static_cast<uint32_t>(WheelCentreOffsets.Num())) ? WheelCentreOffsets[i] : FVector::Zero();
        // Mundi: Z가 Up. 서스펜션 오프셋과 휠 반지름을 Z축에 적용 후 변환.
        WheelCentreLocal.Z += Setup.SuspensionOffsetZ;
        WheelCentreLocal.Z -= Setup.WheelRadius;

        physx::PxVec3 WheelCentreOffset = PhysicsConversion::ToPxVec3(WheelCentreLocal);

        WheelsSimData->setWheelData(i, WheelData);
        WheelsSimData->setSuspensionData(i, SusData);
        WheelsSimData->setWheelCentreOffset(i, WheelCentreOffset);
        WheelsSimData->setSuspTravelDirection(i, physx::PxVec3(0.0f, -1.0f, 0.0f));
        WheelsSimData->setSuspForceAppPointOffset(i, WheelCentreOffset);
        WheelsSimData->setTireForceAppPointOffset(i, WheelCentreOffset);
    }

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

    physx::PxVehicleChassisData ChassisData;
    ChassisData.mMass = VehicleMass;
    ChassisData.mMOI = physx::PxVec3(200.0f, 200.0f, 200.0f);
    ChassisData.mCMOffset = physx::PxVec3(0.0f, -0.2f, 0.0f);

    physx::PxVehicleSetBasisVectors(physx::PxVec3(0.0f, 1.0f, 0.0f), physx::PxVec3(0.0f, 0.0f, -1.0f));
    physx::PxVehicleSetUpdateMode(physx::PxVehicleUpdateMode::eVELOCITY_CHANGE);

    physx::PxVehicleDrive4W* Drive4W = physx::PxVehicleDrive4W::allocate(NumWheels);
    Drive4W->setup(Physics, ChassisActor, *WheelsSimData, DriveSimData, NumWheels);
    WheelsSimData->free();

    // setup은 실패시
    if (!Drive4W->getRigidDynamicActor())
    {
        Drive4W->free();
        UE_LOG("[VehicleMovement] PxVehicleDrive4W setup failed (no actor bound).");
        return false;
    }

    PxVehicleDrive4WInstance = Drive4W;
    PxVehicleWheelsInstance = Drive4W;
    PxVehicleActor = ChassisActor;

    bVehicleInitialized = true;
    UE_LOG("[VehicleMovement] Vehicle PhysX initialization complete.");

    // 초기 휠 본 로컬 위치 캐싱 (스켈레탈 메시에만 적용)
    if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(UpdatedComponent))
    {
        InitialWheelLocalPositions.Empty();
        InitialWheelLocalPositions.SetNum(WheelSetups.Num());

        USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMesh();
        const FSkeleton* Skeleton = SkelMesh ? SkelMesh->GetSkeletalMeshData() ? &SkelMesh->GetSkeletalMeshData()->Skeleton : nullptr : nullptr;
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
                    // 컴포넌트 공간에서 본 트랜스폼을 얻어 차체(UpdatedComponent) 로컬로 변환
                    FTransform BoneWorld = SkelComp->GetBoneWorldTransform(BoneIndex);
                    FTransform ChassisWorld = UpdatedComponent->GetWorldTransform();
                    FTransform BoneLocalToChassis = BoneWorld.GetRelativeTransform(ChassisWorld);
                    InitialWheelLocalPositions[WheelIdx] = BoneLocalToChassis.Translation;
                }
                else
                {
                    InitialWheelLocalPositions[WheelIdx] = FVector::Zero();
                }
            }
        }
    }
    return true;
}

void USimpleWheeledVehicleMovementComponent::ApplyInputToPhysX(float DeltaTime)
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

void USimpleWheeledVehicleMovementComponent::PerformSuspensionRaycasts()
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

        WheelCentreLocal.Z += Setup.SuspensionOffsetZ;
        WheelCentreLocal.Z -= Setup.WheelRadius;

        const FVector StartMundi = ChassisWorld.TransformPosition(WheelCentreLocal);
        const FVector DirMundi = FVector(0.0f, 0.0f, -1.0f); // 다운 방향

        const float TraceLength = FMath::Max(Setup.SuspensionOffsetZ + Setup.WheelRadius + 50.0f, 50.0f);

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

void USimpleWheeledVehicleMovementComponent::SimulateVehicle(float DeltaTime)
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

void USimpleWheeledVehicleMovementComponent::UpdateVehiclePoseFromPhysX()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance || !UpdatedComponent)
    {
        return;
    }

    if (!PxVehicleActor)
    {
        return;
    }

    // PhysX 차체 글로벌 포즈를 엔진 Transform으로 변환하여 UpdatedComponent에 적용
    physx::PxTransform PxPose = PxVehicleActor->getGlobalPose();
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

void USimpleWheeledVehicleMovementComponent::EnsureUpdatedComponentIsValid()
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

void USimpleWheeledVehicleMovementComponent::CleanupVehiclePhysX()
{
    if (PxVehicleDrive4WInstance)
    {
        PxVehicleDrive4WInstance->free();
        PxVehicleDrive4WInstance = nullptr;
    }

    PxVehicleWheelsInstance = nullptr;

    if (TireFrictionPairs)
    {
        TireFrictionPairs->release();
        TireFrictionPairs = nullptr;
    }

    WheelQueryResults.clear();
    VehicleQueryResults.wheelQueryResults = nullptr;
    VehicleQueryResults.nbWheelQueryResults = 0;

    // 차체 Actor는 BodyInstance가 소유하므로 여기서 해제/씬 제거하지 않음
    PxVehicleActor = nullptr;

    bVehicleInitialized = false;
    bWarnedMissingBodyComponent = false;
    bWarnedPhysicsUninitialized = false;
    bWarnedWheelSetup = false;
}

void USimpleWheeledVehicleMovementComponent::RegisterWithPhysScene()
{
    if (bRegisteredWithPhysScene)
    {
        return;
    }

    UWorld* World = nullptr;
    if (UpdatedComponent)
    {
        World = UpdatedComponent->GetWorld();
    }

    if (!World && Owner)
    {
        if (AActor* OwnerActor = Cast<AActor>(Owner))
        {
            World = OwnerActor->GetWorld();
        }
    }

    if (!World)
    {
		UE_LOG("[VehicleMovement] Failed to register vehicle component: World is null.");
        return;
    }

    if (FPhysScene* PhysScene = World->GetPhysScene())
    {
        PhysScene->RegisterVehicleComponent(this);
        bRegisteredWithPhysScene = true;
    }
    else
    {
		UE_LOG("[VehicleMovement] Failed to register vehicle component: PhysScene is null.");
    }
}

void USimpleWheeledVehicleMovementComponent::UnregisterFromPhysScene()
{
    if (!bRegisteredWithPhysScene)
    {
        return;
    }

    UWorld* World = nullptr;
    if (UpdatedComponent)
    {
        World = UpdatedComponent->GetWorld();
    }

    if (!World && Owner)
    {
        if (AActor* OwnerActor = Cast<AActor>(Owner))
        {
            World = OwnerActor->GetWorld();
        }
    }

    if (FPhysScene* PhysScene = World ? World->GetPhysScene() : nullptr)
    {
        PhysScene->UnregisterVehicleComponent(this);
    }

    bRegisteredWithPhysScene = false;
}
