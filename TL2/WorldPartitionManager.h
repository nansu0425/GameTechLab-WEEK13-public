#pragma once
#include "Vector.h"

class UPrimitiveComponent;
class AActor;

struct FRay;
struct FBound;

class UWorldPartitionManager
{
public:
    UWorldPartitionManager() = default;
    ~UWorldPartitionManager() = default;

    void Register(UPrimitiveComponent* Comp);
    void Unregister(UPrimitiveComponent* Comp);

    void MarkDirty(UPrimitiveComponent* Comp);

    void Update(float DeltaTime, uint32 budgetItems = 256);

    void Query(FRay InRay);
    void Query(FBound InBound);
    //...

private:

private:
    //For FIFO Update
    TQueue<UPrimitiveComponent*> DirtyQueue;
    //For Avoid Duplication
    TSet<UPrimitiveComponent*> DirtySet;
};