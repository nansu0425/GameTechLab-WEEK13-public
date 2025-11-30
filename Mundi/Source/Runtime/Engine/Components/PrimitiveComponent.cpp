#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
#include "BodySetup.h"
#include "PhysScene.h"
#include "World.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
}

void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
    if (InWorld)
    {
        if (UWorldPartitionManager* Partition = InWorld->GetPartitionManager())
        {
            Partition->Register(this);
        }
    }
}

void UPrimitiveComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // BodyInstance 물리 파라미터 직렬화
    // (런타임 포인터들은 BeginPlay에서 CreatePhysicsState로 생성됨)
    if (bInIsLoading)
    {
        // 래퍼 프로퍼티로 읽기
        FJsonSerializer::ReadBool(InOutHandle, "bSimulatePhysics", bSimulatePhysics, false, false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnableGravity", bEnableGravity, true, false);
        FJsonSerializer::ReadBool(InOutHandle, "bIsTrigger", bIsTrigger, false, false);
        FJsonSerializer::ReadFloat(InOutHandle, "MassInKg", BodyInstance.MassInKg, 0.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "LinearDamping", BodyInstance.LinearDamping, 0.01f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "AngularDamping", BodyInstance.AngularDamping, 0.05f, false);

        // 래퍼 프로퍼티 → BodyInstance 동기화
        BodyInstance.bSimulatePhysics = bSimulatePhysics;
        BodyInstance.bEnableGravity = bEnableGravity;
        BodyInstance.bIsTrigger = bIsTrigger;
    }
    else
    {
        // BodyInstance → 래퍼 프로퍼티 동기화 (저장 전)
        bSimulatePhysics = BodyInstance.bSimulatePhysics;
        bEnableGravity = BodyInstance.bEnableGravity;
        bIsTrigger = BodyInstance.bIsTrigger;

        InOutHandle["bSimulatePhysics"] = bSimulatePhysics;
        InOutHandle["bEnableGravity"] = bEnableGravity;
        InOutHandle["bIsTrigger"] = bIsTrigger;
        InOutHandle["MassInKg"] = BodyInstance.MassInKg;
        InOutHandle["LinearDamping"] = BodyInstance.LinearDamping;
        InOutHandle["AngularDamping"] = BodyInstance.AngularDamping;
    }
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.Other)
        {
            if (AActor* Owner = Info.Other->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// 물리 시스템
// ═══════════════════════════════════════════════════════════════════════════

void UPrimitiveComponent::BeginPlay()
{
    Super::BeginPlay();

    // 에디터에서 설정한 래퍼 프로퍼티 → BodyInstance 동기화
    // (에디터에서 UPROPERTY 값을 변경하면 래퍼 프로퍼티만 변경되므로)
    BodyInstance.bSimulatePhysics = bSimulatePhysics;
    BodyInstance.bEnableGravity = bEnableGravity;
    BodyInstance.bIsTrigger = bIsTrigger;

    CreatePhysicsState();
}

void UPrimitiveComponent::EndPlay()
{
    DestroyPhysicsState();
    Super::EndPlay();
}

void UPrimitiveComponent::CreatePhysicsState()
{
    // BodySetup이 없으면 물리 상태 생성 안 함
    UBodySetup* Setup = GetBodySetup();
    if (!Setup)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (FPhysScene* PhysScene = World->GetPhysScene())
        {
            BodyInstance.InitBody(Setup, GetWorldTransform(), this, PhysScene);
        }
    }
}

void UPrimitiveComponent::DestroyPhysicsState()
{
    BodyInstance.TermBody();
}

void UPrimitiveComponent::SetSimulatePhysics(bool bSimulate)
{
    BodyInstance.bSimulatePhysics = bSimulate;
    if (BodyInstance.IsInitialized())
    {
        BodyInstance.SetSimulatePhysics(bSimulate);
    }
}

void UPrimitiveComponent::SetIsTrigger(bool bTrigger)
{
    BodyInstance.bIsTrigger = bTrigger;
    // 이미 생성된 경우 재생성 필요 (또는 Shape 플래그 수정)
}
