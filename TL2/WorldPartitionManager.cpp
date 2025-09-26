#include "pch.h"
#include "WorldPartitionManager.h"
#include "PrimitiveComponent.h"
#include "Actor.h"
#include "AABoundingBoxComponent.h"

void UWorldPartitionManager::Register(UPrimitiveComponent* Comp)
{
    if (!Comp) return;

    if (DirtySet.insert(Comp).second)
    {
        DirtyQueue.push(Comp);
    }
}

void UWorldPartitionManager::Unregister(UPrimitiveComponent* Comp)
{
    if (!Comp) return;

    DirtySet.erase(Comp);

    //Tree에 Component 리무브 포워딩
    
    //Octree.Remove(~)

}

void UWorldPartitionManager::MarkDirty(UPrimitiveComponent* Comp)
{
    if (!Comp) return;
    if (DirtySet.insert(Comp).second)
    {
        DirtyQueue.push(Comp);
    }
}

void UWorldPartitionManager::Update(float DeltaTime, uint32 InBugetCount)
{
    // 프레임 히칭 방지를 위해 컴포넌트 카운트 제한
    uint32 processed = 0;
    while (!DirtyQueue.empty() && processed < InBugetCount)
    {
        UPrimitiveComponent* Comp = DirtyQueue.front();
        DirtyQueue.pop();
        if (DirtySet.erase(Comp) == 0)
        {
            // 이미 처리되었거나 제거됨 → 스킵
            continue;
        }

        // 현재 월드 AABB를 컴포넌트(혹은 Collision 컴포넌트)에서 가져옵니다.
        if (Comp && Comp->GetOwner() && Comp->GetOwner()->CollisionComponent)
        {
            //Tree에 Component 업데이트 포워딩

            //Octree.Update(~)

        }
        ++processed;
    }
}

void UWorldPartitionManager::Query(FRay InRay)
{

}

void UWorldPartitionManager::Query(FBound InBound)
{
}
