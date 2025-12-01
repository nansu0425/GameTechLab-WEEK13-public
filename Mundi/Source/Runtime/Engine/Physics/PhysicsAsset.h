#pragma once
#include "Object.h"
#include "UPhysicsAsset.generated.h"
#include "ConstraintInstance.h"

class UBodySetup;

// 충돌 무시 테이블의 해시
struct FPairHash
{
    template<class T1, class T2>
    std::size_t operator()(const TPair<T1, T2>& p) const
    {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

UCLASS(DisplayName = "물리 애셋", Description = "물리 애셋")
class UPhysicsAsset : public UObject
{
public:
    GENERATED_REFLECTION_BODY()
    UPhysicsAsset();
    ~UPhysicsAsset();

    void UpdateBodySetupIndexMap();

    int32 FindBodyIndex(const FName& BodyName) const;

    bool IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const;

    void DisableCollision(int32 BodyIndexA, int32 BodyIndexB);
    void EnableCollision(int32 BodyIndexA, int32 BodyIndexB);

private:
    TArray<UBodySetup*> BodySetups;
    
    TArray<FConstraintInstance> ConstraintSetups;

    // 충돌 무시 테이블
    // 같은 테이블에 저장된 인덱스 끼리는 충돌하지 않음
    TSet<TPair<int32, int32>, FPairHash> CollisionDisableTable;

    TMap<FName, int32> BoneNameToBodyIndex;
};