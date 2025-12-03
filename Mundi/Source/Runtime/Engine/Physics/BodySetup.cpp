#include "pch.h"
#include "BodySetup.h"
#include "PhysicsTypes.h"
#include "PhysicsCore.h"
#include "AggregateGeom.h"
#include <PxPhysicsAPI.h>

using namespace physx;

const FVector UBodySetup::DefaultBoxExtent = FVector(0.5f, 0.5f, 0.5f);
const float UBodySetup::DefaultSphereRadius = 1.0f;
const float UBodySetup::DefaultCapsuleRadius = 0.5f;
const float UBodySetup::DefaultCapsuleHalfHeight = 1.0f;

// ═══════════════════════════════════════════════════════════════════════════
// 생성자
// ═══════════════════════════════════════════════════════════════════════════

UBodySetup::UBodySetup()
    : BoxExtent(DefaultBoxExtent)
    , SphereRadius(DefaultSphereRadius)
    , CapsuleHalfHeight(DefaultCapsuleHalfHeight)
{
}

// ═══════════════════════════════════════════════════════════════════════════
// AggGeom 자동 생성 헬퍼
// ═══════════════════════════════════════════════════════════════════════════

bool UBodySetup::IsAggGeomEmpty() const
{
    return AggGeom.BoxElems.IsEmpty()
        && AggGeom.SphereElems.IsEmpty()
        && AggGeom.SphylElems.IsEmpty()
        && AggGeom.ConvexElems.IsEmpty();
}

void UBodySetup::GenerateAggGeomFromProperties() const
{
    // const_cast: fallback 자동 생성은 논리적 const (lazy initialization)
    FAggregateGeom& MutableAggGeom = const_cast<FAggregateGeom&>(AggGeom);

    switch (BodyType)
    {
    case EBodySetupType::Box:
        {
            FBoxElem BoxElem;
            BoxElem.X = BoxExtent.X * 2.0f;  // half → full
            BoxElem.Y = BoxExtent.Y * 2.0f;
            BoxElem.Z = BoxExtent.Z * 2.0f;
            BoxElem.Center = FVector::Zero();
            BoxElem.Rotation = FQuat::Identity();
            MutableAggGeom.BoxElems.Add(BoxElem);
        }
        break;

    case EBodySetupType::Sphere:
        {
            FSphereElem SphereElem;
            SphereElem.Radius = SphereRadius;
            SphereElem.Center = FVector::Zero();
            MutableAggGeom.SphereElems.Add(SphereElem);
        }
        break;

    case EBodySetupType::Capsule:
        {
            FSphylElem SphylElem;
            SphylElem.Radius = SphereRadius;
            // CapsuleHalfHeight는 언리얼 방식 (반구 포함 전체 높이의 절반)
            // FSphylElem.Length는 실린더 부분의 전체 길이
            SphylElem.Length = FMath::Max(0.0f, (CapsuleHalfHeight - SphereRadius) * 2.0f);
            SphylElem.Center = FVector::Zero();
            SphylElem.Rotation = FQuat::Identity();
            MutableAggGeom.SphylElems.Add(SphylElem);
        }
        break;

    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Shape 생성
// ═══════════════════════════════════════════════════════════════════════════

int32 UBodySetup::AddShapesToRigidActor(physx::PxRigidActor* RigidActor, physx::PxMaterial* DefaultMaterial, const FVector& Scale) const
{
    if (!RigidActor)
    {
        return 0;
    }

    // AggGeom Fallback: 비어있으면 직접 프로퍼티에서 자동 생성
    if (IsAggGeomEmpty() && BodyType != EBodySetupType::None)
    {
        GenerateAggGeomFromProperties();
    }

    AddBoxElems(RigidActor, DefaultMaterial, Scale);
    AddSphereElems(RigidActor, DefaultMaterial, Scale);
    AddSphylElems(RigidActor, DefaultMaterial, Scale);
    AddConvexElems(RigidActor, DefaultMaterial, Scale);

    return static_cast<int32>(RigidActor->getNbShapes());
}

void UBodySetup::AddBoxElems(physx::PxRigidActor* RigidActor, physx::PxMaterial* DefaultMaterial, const FVector& Scale) const
{
    PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    for (const auto& BoxElem : AggGeom.BoxElems)
    {
        // Mundi 좌표계 기준 HalfExtent (스케일 적용)
        FVector MundiHalfExtent(
            (BoxElem.X * 0.5f) * FMath::Abs(Scale.X),
            (BoxElem.Y * 0.5f) * FMath::Abs(Scale.Y),
            (BoxElem.Z * 0.5f) * FMath::Abs(Scale.Z)
        );

        // 좌표계 변환: ScaleToPxVec3 사용 (크기이므로 부호 반전 없이 축만 재배치)
        PxVec3 PxHalfExtent = PhysicsConversion::ScaleToPxVec3(MundiHalfExtent);
        PxBoxGeometry BoxGeom(PxHalfExtent.x, PxHalfExtent.y, PxHalfExtent.z);

        PxShape* Shape = Physics->createShape(BoxGeom, *DefaultMaterial, true);
        if (Shape)
        {
            FTransform BoxTransform(BoxElem.Center, BoxElem.Rotation, FVector::One());
            Shape->setLocalPose(PhysicsConversion::ToPxTransform(BoxTransform));

            FBoxElem& MutableElem = const_cast<FBoxElem&>(BoxElem);
            Shape->userData = &MutableElem.UserData;

            RigidActor->attachShape(*Shape);            
            // 액터가 Shape의 소유권을 가짐
            Shape->release();
        }
    }
}

void UBodySetup::AddSphereElems(physx::PxRigidActor* RigidActor, physx::PxMaterial* DefaultMaterial, const FVector& Scale) const
{
    PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    for (const auto& SphereElem : AggGeom.SphereElems)
    {
        float MaxScale = Scale.GetMaxValue();
        float Radius = SphereElem.Radius * MaxScale;

        if (Radius <= 0.0f)
        {
            continue;
        }

        PxSphereGeometry SphereGeom(Radius);

        PxShape* Shape = Physics->createShape(SphereGeom, *DefaultMaterial, true);
        if (Shape)
        {
            PxTransform LocalPose(PhysicsConversion::ToPxVec3(SphereElem.Center));
            Shape->setLocalPose(LocalPose);

            FSphereElem& MutableElem = const_cast<FSphereElem&>(SphereElem);
            Shape->userData = &MutableElem.UserData;

            RigidActor->attachShape(*Shape);
            Shape->release();
        }
    }
}

void UBodySetup::AddSphylElems(physx::PxRigidActor* RigidActor, physx::PxMaterial* DefaultMaterial, const FVector& Scale) const
{
    PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    for (const auto& SphylElem : AggGeom.SphylElems)
    {
        float MaxScale = Scale.GetMaxValue();
        float Radius = SphylElem.Radius * MaxScale;
        float ScaledHalfLength = SphylElem.Length * 0.5f * MaxScale;

        if (Radius <= 0.0f)
        {
            continue;
        }

        PxCapsuleGeometry CapsuleGeom(Radius, ScaledHalfLength);

        PxShape* Shape = Physics->createShape(CapsuleGeom, *DefaultMaterial, true);
        if (Shape)
        {
            // PhysX Capsule은 기본적으로 PhysX X축 방향으로 누워있음
            // Mundi Z축(Up) = PhysX Y축이므로, PhysX Y축 방향으로 세워야 함
            // PhysX Z축 기준 90도 회전: PhysX X축 → PhysX Y축
            PxQuat CapsuleUpright(PxHalfPi, PxVec3(0, 0, 1));

            // SphylElem의 Transform 적용
            PxVec3 PxCenter = PhysicsConversion::ToPxVec3(SphylElem.Center);
            PxQuat PxRotation = PhysicsConversion::ToPxQuat(SphylElem.Rotation);

            // 최종 LocalPose = 사용자 회전 * 캡슐 세우기 회전
            PxTransform LocalPose(PxCenter, PxRotation * CapsuleUpright);
            Shape->setLocalPose(LocalPose);

            FSphylElem& MutableElem = const_cast<FSphylElem&>(SphylElem);
            Shape->userData = &MutableElem.UserData;

            RigidActor->attachShape(*Shape);
            Shape->release();
        }
    }
}

void UBodySetup::AddConvexElems(physx::PxRigidActor* RigidActor, physx::PxMaterial* DefaultMaterial, const FVector& Scale) const
{
    physx::PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    physx::PxCooking* Cooking = FPhysicsCore::Get().GetCooking();
    if (!Physics || !Cooking)
    {
        return;
    }

    for (const auto& ConvexElem : AggGeom.ConvexElems)
    {
        physx::PxConvexMesh* TargetMesh = ConvexElem.ConvexMesh;
        // 캐싱이 없으면 쿠킹 실행
        if (!TargetMesh)
        {
            if (ConvexElem.VertexData.IsEmpty())
            {
                continue;
            }

            TArray<physx::PxVec3> PxVertices;
            PxVertices.Reserve(ConvexElem.VertexData.Num());
            // FVector -> PxVec3
            for (const auto& Vertex : ConvexElem.VertexData)
            {
                PxVertices.Add(PhysicsConversion::ToPxVec3(Vertex));
            }

            // Convex Mesh Desciptor 설정
            physx::PxConvexMeshDesc ConvexDesc;
            ConvexDesc.points.count = PxVertices.Num();
            ConvexDesc.points.stride = sizeof(physx::PxVec3);
            ConvexDesc.points.data = PxVertices.GetData();

            // 정점만 넣고 면은 PhysX가 담당
            ConvexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

            // 쿠킹
            physx::PxDefaultMemoryOutputStream Ostream;
            physx::PxConvexMeshCookingResult::Enum Result;
            if (!Cooking->cookConvexMesh(ConvexDesc, Ostream, &Result))
            {
                UE_LOG("[UBodySetup/AddConvexElems] fail convex cooking");
                continue;
            }

            // PxConvexMesh 생성
            physx::PxDefaultMemoryInputData Input(Ostream.getData(), Ostream.getSize());
            TargetMesh = Physics->createConvexMesh(Input);

            // 캐싱
            if (TargetMesh)
            {
                ConvexElem.ConvexMesh = TargetMesh;
            }
        }

        if (TargetMesh)
        {
            // 스케일 변환: 축 재배치만 수행 (부호 반전 없음)
            physx::PxMeshScale PxScale(
                PhysicsConversion::ScaleToPxVec3(Scale),
                physx::PxQuat(physx::PxIdentity)
            );

            PxConvexMeshGeometry ConvexGeom(TargetMesh, PxScale);
            PxShape* Shape = Physics->createShape(ConvexGeom, *DefaultMaterial, true);
            if (Shape)
            {
                Shape->setLocalPose(PhysicsConversion::ToPxTransform(ConvexElem.Transform));

                FConvexElem& MutableElem = const_cast<FConvexElem&>(ConvexElem);
                Shape->userData = &MutableElem.UserData;

                RigidActor->attachShape(*Shape);
                Shape->release();
            }
        }        
    }
}
