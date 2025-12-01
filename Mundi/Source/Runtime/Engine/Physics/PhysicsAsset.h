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

struct FPhysicsAssetSolverSettings
{
    /*
     * 위치 보정
     * 높을 수록 관절 늘어짐이나 분리 현상 감소
     * 낮을 수록 성능 좋지만 랙돌이 흐물거릴 수 있음
     */
    int32 PositionIterations = 5;

    /*
     * 속도 보정
     * 높을 수록 충돌 반발, 마찰력이 정확
     * 권장 1 ~ 2 (일반적으로 position보다 적게 설정)
     */
    int32 VelocityIterations = 2;

    /*
     * 최대 각속도
     * PhysX 기본값은 7 ~ 10 정도
     */
    float MaxAngularVelocity = 10.0f;

    /*
     * 최대 침투 보정 속도
     * 물체가 겹칠 시 밀어내는 속도 한계
     * 높게 잡으면 총알처럼 튕김
     */
    float MaxDepenetrationVelocity = 5.0f;

    /*
     * 운동 에너지 임계값
     * 이 값보다 낮으면 연산 중단
     * PhysX 기본 값 0.005 * mass
     */
    float SleepThreshold = 0.05f;

    /*
     * 안정화 플래그
     * Jitter 방지용 추가 연산 수행 여부
     */
    bool bEnableStabilization = true;

    FPhysicsAssetSolverSettings() = default;

    FPhysicsAssetSolverSettings(
        int32 InPosIter,
        int32 InVelIter,
        float InMaxAngular,
        float InMaxDepenVel,
        float InThreshold,
        bool bInStablization)
        : PositionIterations(InPosIter),
          VelocityIterations(InVelIter),
          MaxAngularVelocity(InMaxAngular),
          MaxDepenetrationVelocity(InMaxDepenVel),
          SleepThreshold(InThreshold),
          bEnableStabilization(bInStablization)
    {}

    FPhysicsAssetSolverSettings(const FPhysicsAssetSolverSettings& Other)
        : PositionIterations(Other.PositionIterations),
          VelocityIterations(Other.VelocityIterations),
          MaxAngularVelocity(Other.MaxAngularVelocity),
          MaxDepenetrationVelocity(Other.MaxDepenetrationVelocity),
          SleepThreshold(Other.SleepThreshold),
          bEnableStabilization(Other.bEnableStabilization)
    {}

    ~FPhysicsAssetSolverSettings() = default;
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

    FPhysicsAssetSolverSettings SolverSettings;
};