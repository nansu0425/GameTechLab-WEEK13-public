#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"
#include "Collision.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "../Scripting/GameObject.h"

IMPLEMENT_CLASS(UShapeComponent)

BEGIN_PROPERTIES(UShapeComponent)
    ADD_PROPERTY(bool, bShapeIsVisible, "Shape", true, "Shape�� ����ȭ �����Դϴ�.")
    ADD_PROPERTY(bool, bShapeHiddenInGame, "Shape", true, "Shape�� PIE ��忡���� ����ȭ �����Դϴ�.")
 END_PROPERTIES()

UShapeComponent::UShapeComponent() : bShapeIsVisible(true), bShapeHiddenInGame(true)
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f); 
}
void UShapeComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();
}

void UShapeComponent::OnTransformUpdated()
{
    GetWorldAABB();

    // Keep BVH up-to-date for broad phase queries
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->MarkDirty(this);
        }
    }

    UpdateOverlaps();
    Super::OnTransformUpdated();
}

void UShapeComponent::UpdateOverlaps()
{
    // ����� ���� UShapeComponent ��ü�� �浹 �˻縦 ������ �� ������ ��
    if (GetClass() == UShapeComponent::StaticClass())
    {
        bGenerateOverlapEvents = false;
    }

    if (!bGenerateOverlapEvents)
    {
        OverlapInfos.clear();
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World) return; 
      
    OverlapNow.clear();

    // Broad Phase: use world BVH to query candidates overlapping our AABB.
    TArray<UPrimitiveComponent*> Candidates;
    if (UWorldPartitionManager* PM = World->GetPartitionManager())
    {
        if (FBVHierarchy* BVH = PM->GetBVH())
        {
            Candidates = BVH->QueryIntersectedComponents(GetWorldAABB());
        }
    }

    if (!Candidates.IsEmpty())
    {
        for (UPrimitiveComponent* Prim : Candidates)
        {
            UShapeComponent* Other = Cast<UShapeComponent>(Prim);
            if (!Other || Other == this) continue;
            if (Other->GetOwner() == this->GetOwner()) continue;
            if (!Other->bGenerateOverlapEvents) continue;
            
            AActor* Owner = this->GetOwner();
            AActor* OtherOwner = Other->GetOwner();

            /*if (Owner && Owner->GetTag() == "fireball"
                && OtherOwner && OtherOwner->GetTag() == "Tile")
            {
                continue;
            }*/

            
            if (Owner && Owner->GetTag() == "Tile"
                && OtherOwner && OtherOwner->GetTag() == "Tile")
            {
                continue;
            }

            // Narrow phase check
            if (!Collision::CheckOverlap(this, Other)) continue;

            OverlapNow.Add(Other);
        }
    }
    else
    {
        // Fallback: O(N^2) scan if BVH is not available
        for (AActor* Actor : World->GetActors())
        {
            for (USceneComponent* Comp : Actor->GetSceneComponents())
            {
                UShapeComponent* Other = Cast<UShapeComponent>(Comp);
                if (!Other || Other == this) continue;
                if (Other->GetOwner() == this->GetOwner()) continue;
                if (!Other->bGenerateOverlapEvents) continue;

                AActor* Owner = this->GetOwner();
                AActor* OtherOwner = Other->GetOwner();

                if (Owner && Owner->GetTag() == "Tile"
                    && OtherOwner && OtherOwner->GetTag() == "Tile")
                {
                    continue;
                }

                if (!Collision::CheckOverlap(this, Other)) continue;

                OverlapNow.Add(Other);
            }
        }
    }

    // Publish current overlaps
    OverlapInfos.clear();
    for (UShapeComponent* Other : OverlapNow)
    {
        FOverlapInfo Info;
        Info.OtherActor = Other->GetOwner();
        Info.Other = Other;
        OverlapInfos.Add(Info);
    } 

    //Begin
    for (UShapeComponent* Comp : OverlapNow)
    {
        if (!OverlapPrev.Contains(Comp))
        {
            Owner->OnComponentBeginOverlap.Broadcast(this, Comp);
            if (AActor* OtherOwner = Comp->GetOwner())
            {
                OtherOwner->OnComponentBeginOverlap.Broadcast(Comp, this);
            }
            
            if (bBlockComponent)
            {
                Owner->OnComponentHit.Broadcast(this, Comp);
            }
        }
    }

    //End
    for (UShapeComponent* Comp : OverlapPrev)
    {
        if (!OverlapNow.Contains(Comp))
        {
            Owner->OnComponentEndOverlap.Broadcast(this, Comp);
        }
    }

    OverlapPrev.clear();
    for (UShapeComponent* Comp : OverlapNow)
    {
        OverlapPrev.Add(Comp);

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

void UShapeComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

