#include "pch.h"
#include "PhysicsAsset.h"

#include "BodySetup.h"

UPhysicsAsset::UPhysicsAsset()
{
}

UPhysicsAsset::~UPhysicsAsset()
{
}

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
    BoneNameToBodyIndex.Empty();
    for (int32 i = 0; i < BodySetups.Num(); i++)
    {
        if (BodySetups[i])
        {
            BoneNameToBodyIndex[BodySetups[i]->BoneName] = i;
        }
    }
}

int32 UPhysicsAsset::FindBodyIndex(const FName& BodyName) const
{    
    if (const int32* IndexPtr = BoneNameToBodyIndex.Find(BodyName))
    {
        return *IndexPtr;
    }
    return -1;
}

bool UPhysicsAsset::IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const
{
    // 자기 자신과 충돌할 수 없다.
    if (BodyIndexA == BodyIndexB)
    {
        return false;
    }

    // 순서 정규화, 작은 값을 앞으로
    if (BodyIndexA > BodyIndexB)
    {
        std::swap(BodyIndexA, BodyIndexB);
    }
    
    if (CollisionDisableTable.Contains(TPair<int32, int32>(BodyIndexA, BodyIndexB)))
    {
        return false;
    }
    
    return true;
}

void UPhysicsAsset::DisableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    if ((BodyIndexA != BodyIndexB) && (BodyIndexA != -1) && (BodyIndexB != -1))
    {
        // 순서 정규화, 작은 값을 앞으로
        if (BodyIndexA > BodyIndexB)
        {
            std::swap(BodyIndexA, BodyIndexB);
        }
        CollisionDisableTable.Add(TPair{BodyIndexA, BodyIndexB});
    }
}

void UPhysicsAsset::EnableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    // 순서 정규화, 작은 값을 앞으로
    if (BodyIndexA > BodyIndexB)
    {
        std::swap(BodyIndexA, BodyIndexB);
    }
    if (CollisionDisableTable.Contains(TPair<int32, int32>(BodyIndexA, BodyIndexB)))
    {
        CollisionDisableTable.Remove(TPair{BodyIndexA, BodyIndexB});
    }
}

void UPhysicsAsset::AddBodySetup(UBodySetup* NewBody)
{
    if (NewBody)
    {
        BodySetups.Add(NewBody);
        BoneNameToBodyIndex[NewBody->BoneName] = BodySetups.Num() - 1;
    }
}

void UPhysicsAsset::ClearAllBodies()
{
    BodySetups.Empty();
    BoneNameToBodyIndex.Empty();
    CollisionDisableTable.Empty();
}