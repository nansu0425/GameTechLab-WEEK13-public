#include "pch.h"
#include "ShapeComponent.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "CollisionManager.h"
#include "BodySetup.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UShapeComponent::UShapeComponent() : bShapeIsVisible(true), bShapeHiddenInGame(true)
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
    bCanEverTick = true;
}

UShapeComponent::~UShapeComponent()
{
    // 자체 BodySetup 명시적 삭제 (Mundi는 GC 없음)
    if (ShapeBodySetup)
    {
        ObjectFactory::DeleteObject(ShapeBodySetup);
        ShapeBodySetup = nullptr;
    }
}

void UShapeComponent::BeginPlay()
{
    Super::BeginPlay();

    // World의 CollisionManager에 등록
    if (UWorld* World = GetWorld())
    {
        if (UCollisionManager* Manager = World->GetCollisionManager())
        {
            Manager->RegisterComponent(this);
        }
    }
}

void UShapeComponent::EndPlay()
{
    // World의 CollisionManager에서 해제
    if (UWorld* World = GetWorld())
    {
        if (UCollisionManager* Manager = World->GetCollisionManager())
        {
            Manager->UnregisterComponent(this);
        }
    }

    Super::EndPlay();
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    GetWorldAABB();

    // World의 CollisionManager에 등록 (에디터에서도 충돌 BVH 표시용)
    if (InWorld)
    {
        if (UCollisionManager* Manager = InWorld->GetCollisionManager())
        {
            Manager->RegisterComponent(this);
        }
    }
}

void UShapeComponent::OnUnregister()
{
    // World의 CollisionManager에서 해제
    if (UWorld* World = GetWorld())
    {
        if (UCollisionManager* Manager = World->GetCollisionManager())
        {
            Manager->UnregisterComponent(this);
        }
    }

    Super::OnUnregister();
}

void UShapeComponent::OnTransformUpdated()
{
    // Bounds 업데이트 (자식 클래스의 CachedBounds 갱신)
    UpdateBounds();

    if (UWorld* World = GetWorld())
    {
        // CollisionManager에 Dirty 마킹 (충돌 BVH용)
        if (UCollisionManager* Manager = World->GetCollisionManager())
        {
            Manager->MarkComponentDirty(this);
        }

        // WorldPartitionManager에도 Dirty 마킹 (기본 BVH용 - 파티클 충돌)
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->MarkDirty(this);
        }
    }

    //UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::TickComponent(float DeltaSeconds)
{
    // PhysX가 모든 물리/Overlap 이벤트를 처리함
    // (PhysicsEventCallback에서 onTrigger/onContact 이벤트를 처리)

    UWorld* World = GetWorld();
    if (!World) return;

    // 매 프레임 Bounds 업데이트 및 BVH dirty 마킹 (에디터에서 속성 직접 수정 시 반영)
    UpdateBounds();
    if (UCollisionManager* Manager = World->GetCollisionManager())
    {
        Manager->MarkComponentDirty(this);
    }
    if (UWorldPartitionManager* Partition = World->GetPartitionManager())
    {
        Partition->MarkDirty(this);
    }
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds;
    }
    return WorldAABB;
}

void UShapeComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // Shape 파라미터가 UPROPERTY로 자동 로드된 후,
        // BodySetup에 반영해야 PhysX Shape가 올바른 크기로 생성됨
        UpdateBodySetup();
    }
}

void UShapeComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // Archetype 사용 여부 재평가
    bUseArchetypeBodySetup = IsUsingDefaultParameters();
    if (!bUseArchetypeBodySetup)
    {
        // 커스텀 파라미터 → 자체 BodySetup 생성
        ShapeBodySetup = ObjectFactory::NewObject<UBodySetup>();
        UpdateBodySetup();
    }
    else
    {
        // 기본값 → Archetype 공유
        ShapeBodySetup = nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// BodySetup Archetype 패턴 구현
// ═══════════════════════════════════════════════════════════════════════

UBodySetup* UShapeComponent::GetBodySetup() const
{
    if (bUseArchetypeBodySetup)
    {
        return GetDefaultBodySetup();  // Static 기본값 공유
    }
    return ShapeBodySetup;  // 자체 BodySetup 사용
}

void UShapeComponent::EnsureBodySetupIsValid()
{
    if (IsUsingDefaultParameters())
    {
        // 기본값 사용 → Archetype 공유
        bUseArchetypeBodySetup = true;
        if (ShapeBodySetup)
        {
            // 기존 자체 BodySetup 명시적 삭제
            ObjectFactory::DeleteObject(ShapeBodySetup);
            ShapeBodySetup = nullptr;
        }
    }
    else
    {
        // 커스텀 값 → 자체 BodySetup 필요
        if (bUseArchetypeBodySetup || !ShapeBodySetup)
        {
            bUseArchetypeBodySetup = false;
            ShapeBodySetup = ObjectFactory::NewObject<UBodySetup>();
        }
    }
}

void UShapeComponent::UpdateBodySetup()
{
    EnsureBodySetupIsValid();
    // 파생 클래스에서 ShapeBodySetup에 실제 값 설정
}




