#include "pch.h"
#include "BodyInstance.h"
#include "BodyInstanceImpl.h"
#include "BodySetup.h"
#include "BodySetupImpl.h"
#include "PhysScene.h"
#include "PhysSceneImpl.h"
#include "PhysicsCore.h"
#include "PhysicsTypes.h"
#include "PhysicsSceneLock.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "GlobalConsole.h"

#include <PxPhysicsAPI.h>

using namespace physx;

// ═══════════════════════════════════════════════════════════════════════════
// 생성자/소멸자
// ═══════════════════════════════════════════════════════════════════════════

FBodyInstance::FBodyInstance()
    : Impl(std::make_unique<FBodyInstanceImpl>()), UserData(this)
{
}

FBodyInstance::~FBodyInstance()
{
    TermBody();
}

FBodyInstance::FBodyInstance(const FBodyInstance& Other)
    : Impl(std::make_unique<FBodyInstanceImpl>())  // 새 Impl 생성
    , OwnerComponent(nullptr)  // 복사 시 소유자는 nullptr
    , CurrentSceneState(EBodyInstanceSceneState::NotAdded)
    , bSimulatePhysics(Other.bSimulatePhysics)
    , bEnableGravity(Other.bEnableGravity)
    , bIsTrigger(Other.bIsTrigger)
    , MassInKg(Other.MassInKg)
    , LinearDamping(Other.LinearDamping)
    , AngularDamping(Other.AngularDamping)
    , OwnerScene(nullptr)
    , UserData(this)
{
    // PhysX Actor는 복사하지 않음 - 나중에 InitBody()로 새로 생성해야 함
}

FBodyInstance& FBodyInstance::operator=(const FBodyInstance& Other)
{
    if (this != &Other)
    {
        // 기존 PhysX Actor 정리
        TermBody();

        // 물리 파라미터 복사
        bSimulatePhysics = Other.bSimulatePhysics;
        bEnableGravity = Other.bEnableGravity;
        bIsTrigger = Other.bIsTrigger;
        MassInKg = Other.MassInKg;
        LinearDamping = Other.LinearDamping;
        AngularDamping = Other.AngularDamping;

        // 소유자/Scene은 복사하지 않음
        OwnerComponent = nullptr;
        OwnerScene = nullptr;
        CurrentSceneState = EBodyInstanceSceneState::NotAdded;

        // Impl은 이미 존재하므로 재생성하지 않음
        // PhysX Actor는 InitBody()로 새로 생성해야 함
    }
    return *this;
}

FBodyInstance::FBodyInstance(FBodyInstance&& Other) noexcept
    : Impl(std::move(Other.Impl)) // 이동 연산자는 자원을 뺏어옴
    , OwnerComponent(Other.OwnerComponent)
    , CurrentSceneState(Other.CurrentSceneState)
    , bSimulatePhysics(Other.bSimulatePhysics)
    , bEnableGravity(Other.bEnableGravity)
    , bIsTrigger(Other.bIsTrigger)
    , MassInKg(Other.MassInKg)
    , LinearDamping(Other.LinearDamping)
    , AngularDamping(Other.AngularDamping)
    , OwnerScene(Other.OwnerScene)
    , UserData(this)
{
    if (Impl && Impl->RigidActorSync)
    {
        Impl->RigidActorSync->userData = &this->UserData;
    }

    // Other는 자원을 뺏겨서 껍데기만 존재함
    Other.OwnerComponent = nullptr;
    Other.CurrentSceneState = EBodyInstanceSceneState::NotAdded;
}

FBodyInstance& FBodyInstance::operator=(FBodyInstance&& Other) noexcept
{
    if (this != &Other)
    {
        // 기존 PhysX Actor 정리
        TermBody();

        Impl = std::move(Other.Impl);
        OwnerComponent = Other.OwnerComponent;
        OwnerScene = Other.OwnerScene;
        CurrentSceneState = Other.CurrentSceneState;

        // 물리 파라미터 복사
        bSimulatePhysics = Other.bSimulatePhysics;
        bEnableGravity = Other.bEnableGravity;
        bIsTrigger = Other.bIsTrigger;
        MassInKg = Other.MassInKg;
        LinearDamping = Other.LinearDamping;
        AngularDamping = Other.AngularDamping;

        if (Impl && Impl->RigidActorSync)
        {
            Impl->RigidActorSync->userData = &this->UserData;
        }

        // 소유자/Scene은 복사하지 않음
        Other.OwnerComponent = nullptr;
        Other.OwnerScene = nullptr;
        Other.CurrentSceneState = EBodyInstanceSceneState::NotAdded;
    }
    return *this;
}

// ═══════════════════════════════════════════════════════════════════════════
// 생명주기
// ═══════════════════════════════════════════════════════════════════════════

void FBodyInstance::InitBody(
    UBodySetup* Setup,
    const FTransform& Transform,
    UPrimitiveComponent* Owner,
    FPhysScene* Scene)
{
    // 유효성 검사
    if (!Setup || !Owner || !Scene)
    {
        UE_LOG("FBodyInstance::InitBody - Invalid parameters (Setup=%p, Owner=%p, Scene=%p)",
            Setup, Owner, Scene);
        return;
    }

    // 이미 초기화되었으면 정리
    if (IsInitialized())
    {
        TermBody();
    }

    // 소유자 설정
    OwnerComponent = Owner;
    OwnerScene = Scene;
    CurrentSceneState = EBodyInstanceSceneState::AwaitingAdd;

    // PhysX 객체 가져오기
    FPhysSceneImpl* SceneImpl = Scene->GetImpl();
    if (!SceneImpl || !SceneImpl->IsInitialized())
    {
        UE_LOG("FBodyInstance::InitBody - PhysScene not initialized");
        CurrentSceneState = EBodyInstanceSceneState::NotAdded;
        return;
    }

    PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    PxScene* PScene = SceneImpl->GetPxScene();
    PxMaterial* DefaultMaterial = SceneImpl->GetDefaultMaterial();

    if (!Physics || !PScene || !DefaultMaterial)
    {
        UE_LOG("FBodyInstance::InitBody - PhysX objects not available");
        CurrentSceneState = EBodyInstanceSceneState::NotAdded;
        return;
    }

    // Transform 변환 (Mundi → PhysX)
    PxTransform PxPose = PhysicsConversion::ToPxTransform(Transform);

    PxRigidActor* NewActor = nullptr;
    if (bSimulatePhysics)
    {
        NewActor = Physics->createRigidDynamic(PxPose);
    }
    else
    {
        NewActor = Physics->createRigidStatic(PxPose);
    }

    if (!NewActor)
    {
        UE_LOG("FBodyInstance::InitBody - Failed to create PxRigidActor");
        CurrentSceneState = EBodyInstanceSceneState::NotAdded;
        return;
    }
    Impl->RigidActorSync = NewActor;

    // Shape 생성 및 부착
    FVector Scale = Transform.Scale3D;
    int32 ShapeCount = Setup->AddShapesToRigidActor(NewActor, DefaultMaterial, Scale);
    // 만들어진 Shape이 없으면 액터가 의미없다.
    if (ShapeCount == 0)
    {
        NewActor->release();
        NewActor = nullptr;
        Impl->RigidActorSync = nullptr;
        UE_LOG("FBodyInstance::InitBody - Failed to create any shapes via BodySetup");
        CurrentSceneState = EBodyInstanceSceneState::NotAdded;
        return;
    }
    UpdatePhysicsShapeCollision();

    if (bSimulatePhysics)
    {
        PxRigidDynamic* DynamicActor = static_cast<PxRigidDynamic*>(NewActor);
        DynamicActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !bEnableGravity);
        DynamicActor->setLinearDamping(LinearDamping);
        DynamicActor->setAngularDamping(AngularDamping);

        // 질량 설정
        if (MassInKg > 0.0f)
        {
            PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, MassInKg);
        }
        else
        {
            // 자동 질량 계산 (밀도 1000 kg/m³)
            PxRigidBodyExt::updateMassAndInertia(*DynamicActor, 1000.0f);
        }
    }

    NewActor->userData = &this->UserData;
    SceneImpl->AddActor(NewActor);
    CurrentSceneState = EBodyInstanceSceneState::Added;

    UE_LOG("FBodyInstance::InitBody - Body created successfully (Simulate=%s, Trigger=%s)",
        bSimulatePhysics ? "true" : "false",
        bIsTrigger ? "true" : "false");
}

void FBodyInstance::TermBody()
{
    if (!Impl || !Impl->RigidActorSync)
    {
        return;
    }

    CurrentSceneState = EBodyInstanceSceneState::AwaitingRemove;

    // Scene에서 제거 (스레드 안전, 시뮬레이션 중이면 지연 처리)
    if (OwnerScene)
    {
        FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
        if (SceneImpl && SceneImpl->IsInitialized())
        {
            SceneImpl->RemoveActor(Impl->RigidActorSync);
        }
    }

    // Actor 해제
    Impl->RigidActorSync->release();
    Impl->RigidActorSync = nullptr;

    CurrentSceneState = EBodyInstanceSceneState::Removed;
    OwnerComponent = nullptr;
    OwnerScene = nullptr;

    UE_LOG("FBodyInstance::TermBody - Body destroyed");
}

bool FBodyInstance::IsInitialized() const
{
    return Impl && Impl->RigidActorSync != nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Transform 동기화
// ═══════════════════════════════════════════════════════════════════════════

FTransform FBodyInstance::GetWorldTransform() const
{
    if (!Impl || !Impl->RigidActorSync)
    {
        return FTransform();
    }

    // 읽기 잠금으로 Transform 접근 보호
    PxScene* PScene = Impl->RigidActorSync->getScene();
    SCOPED_SCENE_READ_LOCK(PScene);

    PxTransform PxPose = Impl->RigidActorSync->getGlobalPose();
    FTransform Result = PhysicsConversion::ToFTransform(PxPose);

    // 스케일은 Component에서 가져옴
    if (OwnerComponent)
    {
        Result.Scale3D = OwnerComponent->GetWorldScale();
    }

    return Result;
}

void FBodyInstance::SetWorldTransform(const FTransform& NewTransform, bool bTeleport)
{
    if (!Impl || !Impl->RigidActorSync || !OwnerScene)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxTransform PxPose = PhysicsConversion::ToPxTransform(NewTransform);

    // SetGlobalPose를 PhysScene을 통해 호출 (지연 처리 지원)
    SceneImpl->SetActorGlobalPose(Impl->RigidActorSync, PxPose);

    // 텔레포트 시 속도 초기화
    if (bSimulatePhysics && bTeleport)
    {
        PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
        if (Dynamic)
        {
            SceneImpl->SetActorLinearVelocity(Dynamic, PxVec3(0), false);
            SceneImpl->SetActorAngularVelocity(Dynamic, PxVec3(0), false);
        }
    }
}

void FBodyInstance::SyncComponentToPhysics()
{
    if (!OwnerComponent || !Impl || !Impl->RigidActorSync)
    {
        return;
    }

    // 시뮬레이션 중이 아닐 때만 Component → PhysX 동기화
    if (!bSimulatePhysics)
    {
        FTransform CompTransform = OwnerComponent->GetWorldTransform();
        SetWorldTransform(CompTransform, false);
    }
}

void FBodyInstance::SyncPhysicsToComponent()
{
    if (!OwnerComponent || !Impl || !Impl->RigidActorSync)
    {
        return;
    }

    // 시뮬레이션 중일 때만 PhysX → Component 동기화
    if (bSimulatePhysics)
    {
        FTransform PhysTransform = GetWorldTransform();
        OwnerComponent->SetWorldTransform(PhysTransform);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 렌더 보간
// ═══════════════════════════════════════════════════════════════════════════

void FBodyInstance::CapturePhysicsTransform()
{
    if (!Impl || !Impl->RigidActorSync)
    {
        return;
    }

    // 이전 Transform ← 현재 Transform
    Impl->PreviousPhysicsTransform = Impl->CurrentPhysicsTransform;

    // 현재 Transform ← PhysX
    Impl->CurrentPhysicsTransform = Impl->RigidActorSync->getGlobalPose();

    // 최소 1회 캡처 후 보간 활성화
    Impl->bRenderInterpolationValid = true;
}

void FBodyInstance::UpdateRenderInterpolation(float Alpha)
{
    if (!Impl || !Impl->bRenderInterpolationValid || !OwnerComponent)
    {
        return;
    }

    // Alpha 클램프
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // PhysX Transform → Mundi Transform 변환
    FTransform PrevTransform = PhysicsConversion::ToFTransform(Impl->PreviousPhysicsTransform);
    FTransform CurrTransform = PhysicsConversion::ToFTransform(Impl->CurrentPhysicsTransform);

    // 스케일은 Component에서 가져옴 (물리 시뮬레이션에는 스케일 미포함)
    FVector Scale = OwnerComponent->GetWorldScale();
    PrevTransform.Scale3D = Scale;
    CurrTransform.Scale3D = Scale;

    // FTransform::Lerp 사용 (내부적으로 FQuat::Slerp 호출)
    FTransform InterpolatedTransform = FTransform::Lerp(PrevTransform, CurrTransform, Alpha);

    // Component에 적용
    OwnerComponent->SetWorldTransform(InterpolatedTransform);
}

void FBodyInstance::CapturePhysicsVelocity()
{
    if (!Impl)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    // Velocity 캐시 (fetchResults 직후 호출되므로 안전)
    Impl->CachedLinearVelocity = Dynamic->getLinearVelocity();
    Impl->CachedAngularVelocity = Dynamic->getAngularVelocity();
}

void FBodyInstance::UpdatePhysicsShapeCollision()
{
    if (!Impl || !Impl->RigidActorSync)
    {
        return;
    }
    PxRigidActor* RigidActor = Impl->GetPxRigidActor();
    
    const PxU32 PxShapeCount = RigidActor->getNbShapes();
    TArray<PxShape*> Shapes;
    Shapes.SetNum(PxShapeCount);
    
    RigidActor->getShapes(Shapes.GetData(), PxShapeCount);
    for (PxShape* Shape : Shapes)
    {
        ApplyCollision(Shape);
    }
}

void FBodyInstance::ApplyCollision(physx::PxShape* Shape)
{
    bool bSimulate = false;
    bool bSceneQuery = false;
    bool bTrigger = false;

    ECollisionEnabled Type = ECollisionEnabled::NoCollision;
    if (FShapeElem* ShapeElem = FUserData::Get<FShapeElem>(Shape->userData))
    {
        Type = ShapeElem->GetCollisionEnabled();
    }
    
    switch (Type)
    {
    case ECollisionEnabled::NoCollision:        
        break;
    case ECollisionEnabled::QueryOnly:
        {
            bSceneQuery = true;
        }
        break;
    case ECollisionEnabled::PhysicsOnly:
        {
            bSimulate = true;
        }
        break;
    case ECollisionEnabled::QueryAndPhysics:
        {
            bSimulate = true;
            bSceneQuery = true;
        }
        break;
    default:
        break;
    }

    if (bIsTrigger)
    {
        bSimulate = false;
        bTrigger = true;
        bSceneQuery = true;
    }

    Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bSimulate);
    Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bSceneQuery);
    Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, bTrigger);
    Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
}

void FBodyInstance::ApplyCollision(ECollisionEnabled InCollision)
{
    if (!Impl || !Impl->RigidActorSync)
    {
        return;
    }    
 
    bool bSimulate = false;
    bool bSceneQuery = false;
    bool bTrigger = false;
    
    switch (InCollision)
    {
    case ECollisionEnabled::NoCollision:        
        break;
    case ECollisionEnabled::QueryOnly:
        {
            bSceneQuery = true;
        }
        break;
    case ECollisionEnabled::PhysicsOnly:
        {
            bSimulate = true;
        }
        break;
    case ECollisionEnabled::QueryAndPhysics:
        {
            bSimulate = true;
            bSceneQuery = true;
        }
        break;
    default:
        break;
    }

    if (bIsTrigger)
    {
        bSimulate = false;
        bTrigger = true;
        bSceneQuery = true;
    }

    PxRigidActor* RigidActor = Impl->GetPxRigidActor();
    
    const PxU32 PxShapeCount = RigidActor->getNbShapes();
    TArray<PxShape*> Shapes;
    Shapes.SetNum(PxShapeCount);
    
    RigidActor->getShapes(Shapes.GetData(), PxShapeCount);
    for (PxShape* Shape : Shapes)
    {
        Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bSimulate);
        Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bSceneQuery);
        Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, bTrigger);
        Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 물리 제어
// ═══════════════════════════════════════════════════════════════════════════

void FBodyInstance::SetSimulatePhysics(bool bSimulate)
{
    // 현재 상태와 같으면 무시
    if (bSimulatePhysics == bSimulate)
    {
        return;
    }

    bSimulatePhysics = bSimulate;

    // 이미 초기화된 상태라면 Actor 타입 변경이 필요
    // (Static ↔ Dynamic 변환은 복잡하므로 재생성 권장)
    // 현재 구현에서는 런타임 변경 미지원
    if (Impl && Impl->RigidActorSync)
    {
        UE_LOG("FBodyInstance::SetSimulatePhysics - Runtime change requires re-initialization");
    }
}

void FBodyInstance::AddForce(const FVector& Force, bool bAccelChange)
{
    if (!Impl || !OwnerScene)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxVec3 PxForce = PhysicsConversion::ToPxVec3(Force);
    PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
    SceneImpl->AddForceToActor(Dynamic, PxForce, Mode);
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
    if (!Impl || !OwnerScene)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxVec3 PxImpulse = PhysicsConversion::ToPxVec3(Impulse);
    PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
    SceneImpl->AddForceToActor(Dynamic, PxImpulse, Mode);
}

void FBodyInstance::AddTorqueInRadians(const FVector& Torque, bool bAccelChange)
{
    if (!Impl || !OwnerScene)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxVec3 PxTorque = PhysicsConversion::ToPxVec3(Torque);
    PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
    SceneImpl->AddTorqueToActor(Dynamic, PxTorque, Mode);
}

// ═══════════════════════════════════════════════════════════════════════════
// 속도
// ═══════════════════════════════════════════════════════════════════════════

FVector FBodyInstance::GetLinearVelocity() const
{
    if (!Impl)
    {
        return FVector::Zero();
    }

    // 캐시된 값 반환 (PhysX 직접 접근 안 함 - 스레드 안전)
    return PhysicsConversion::ToFVector(Impl->CachedLinearVelocity);
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity, bool bAddToCurrent)
{
    if (!Impl || !OwnerScene)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxVec3 NewVel = PhysicsConversion::ToPxVec3(Velocity);
    SceneImpl->SetActorLinearVelocity(Dynamic, NewVel, bAddToCurrent);
}

FVector FBodyInstance::GetAngularVelocityInRadians() const
{
    if (!Impl)
    {
        return FVector::Zero();
    }

    // 캐시된 값 반환 (PhysX 직접 접근 안 함 - 스레드 안전)
    return PhysicsConversion::ToFVector(Impl->CachedAngularVelocity);
}

void FBodyInstance::SetAngularVelocityInRadians(const FVector& AngVel, bool bAddToCurrent)
{
    if (!Impl || !OwnerScene)
    {
        return;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return;
    }

    FPhysSceneImpl* SceneImpl = OwnerScene->GetImpl();
    if (!SceneImpl)
    {
        return;
    }

    PxVec3 NewAngVel = PhysicsConversion::ToPxVec3(AngVel);
    SceneImpl->SetActorAngularVelocity(Dynamic, NewAngVel, bAddToCurrent);
}

// ═══════════════════════════════════════════════════════════════════════════
// 질량/관성
// ═══════════════════════════════════════════════════════════════════════════

float FBodyInstance::GetBodyMass() const
{
    if (!Impl)
    {
        return 0.0f;
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return 0.0f;
    }

    return Dynamic->getMass();
}

FVector FBodyInstance::GetBodyInertiaTensor() const
{
    if (!Impl)
    {
        return FVector::Zero();
    }

    PxRigidDynamic* Dynamic = Impl->GetPxRigidDynamic();
    if (!Dynamic)
    {
        return FVector::Zero();
    }

    PxVec3 Inertia = Dynamic->getMassSpaceInertiaTensor();
    return PhysicsConversion::ToFVector(Inertia);
}

physx::PxRigidActor* FBodyInstance::GetPxRigidActor() const
{
    return Impl ? Impl->GetPxRigidActor() : nullptr;
}

physx::PxRigidDynamic* FBodyInstance::GetPxRigidDynamic() const
{
    return Impl ? Impl->GetPxRigidDynamic() : nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// PIMPL 접근자
// ═══════════════════════════════════════════════════════════════════════════

FBodyInstanceImpl* FBodyInstance::GetImpl() const
{
    return Impl.get();
}
