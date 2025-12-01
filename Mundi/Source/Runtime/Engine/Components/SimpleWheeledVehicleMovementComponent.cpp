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
}

USimpleWheeledVehicleMovementComponent::~USimpleWheeledVehicleMovementComponent() = default;

void USimpleWheeledVehicleMovementComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    EnsureUpdatedComponentIsValid();
}

void USimpleWheeledVehicleMovementComponent::OnUnregister()
{
    CleanupVehiclePhysX();
    Super::OnUnregister();
}

void USimpleWheeledVehicleMovementComponent::EndPlay()
{
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

    ApplyInputToPhysX();
    PerformSuspensionRaycasts();
    SimulateVehicle(DeltaTime);
    UpdateVehiclePoseFromPhysX();
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

void USimpleWheeledVehicleMovementComponent::GearUp()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    UE_LOG("[VehicleMovement] GearUp not implemented yet");
}

void USimpleWheeledVehicleMovementComponent::GearDown()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    UE_LOG("[VehicleMovement] GearDown not implemented yet");
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
    physx::PxRigidDynamic* ChassisActor = PrimComp->GetBodyInstanceRef().GetPxRigidDynamic();
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

        FVector WheelCentreLocal = (i < WheelCentreOffsets.Num()) ? WheelCentreOffsets[i] : FVector::Zero();
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
    return true;
}

void USimpleWheeledVehicleMovementComponent::ApplyInputToPhysX()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    // TODO: PxVehicleDrive4WRawInputData에 입력 적용
}

void USimpleWheeledVehicleMovementComponent::PerformSuspensionRaycasts()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    // TODO: PhysX 3.4 이후 deprecated된 PxBatchQuery 대신 PxScene::raycast를 휠 개수만큼 호출하는 방식으로 구현
}

void USimpleWheeledVehicleMovementComponent::SimulateVehicle(float DeltaTime)
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance)
    {
        return;
    }

    // TODO: PxVehicleUpdates 호출
}

void USimpleWheeledVehicleMovementComponent::UpdateVehiclePoseFromPhysX()
{
    if (!bVehicleInitialized || !PxVehicleDrive4WInstance || !UpdatedComponent)
    {
        return;
    }

    // TODO: PhysX 결과를 월드 트랜스폼으로 복사
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

    // 차체 Actor는 BodyInstance가 소유하므로 여기서 해제/씬 제거하지 않음
    PxVehicleActor = nullptr;

    bVehicleInitialized = false;
    bWarnedMissingBodyComponent = false;
    bWarnedPhysicsUninitialized = false;
    bWarnedWheelSetup = false;
}
