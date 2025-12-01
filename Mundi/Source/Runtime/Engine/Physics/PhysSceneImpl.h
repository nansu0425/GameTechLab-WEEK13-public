#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// PhysSceneImpl.h
// FPhysScene의 PhysX 구현부 (PIMPL - 내부 전용)
// ─────────────────────────────────────────────────────────────────────────────
//
// 주의: 이 헤더는 Physics 모듈 내부에서만 사용
//       외부 모듈에서 include 금지
// ─────────────────────────────────────────────────────────────────────────────

#include <PxPhysicsAPI.h>
#include <thread>
#include "UEContainer.h"
#include "PhysicsSceneLock.h"

class UWorld;
class FPhysScene;
class FPhysicsEventCallback;

/**
 * @brief FPhysScene의 PhysX 구현부
 *
 * PIMPL 패턴의 구현 클래스.
 * PhysX 관련 모든 객체와 로직을 담당.
 */
class FPhysSceneImpl
{
public:
    FPhysSceneImpl();
    ~FPhysSceneImpl();

    // ═══════════════════════════════════════════════════════════════════════
    // 초기화/종료
    // ═══════════════════════════════════════════════════════════════════════

    bool Initialize(FPhysScene* InOwnerScene, UWorld* InOwningWorld);
    void Terminate();
    bool IsInitialized() const { return bInitialized; }

    // ═══════════════════════════════════════════════════════════════════════
    // 시뮬레이션
    // ═══════════════════════════════════════════════════════════════════════

    void StartFrame();
    void Simulate(float DeltaSeconds);
    void FetchResults();
    bool IsSimulating() const { return bIsSimulating; }

    /** Active Actors의 Transform을 Component에 동기화 (레거시, 보간 미사용 시) */
    void SyncActiveActorsToComponents();

    // ═══════════════════════════════════════════════════════════════════════
    // 렌더 보간 (고프레임 렌더링 지원)
    // ═══════════════════════════════════════════════════════════════════════

    /** Active Actors의 Transform 이력 캡처 (물리 스텝 후 호출) */
    void CaptureActiveActorsTransform();

    /** Active Actors의 렌더 보간 Transform 업데이트 (매 프레임 호출) */
    void UpdateRenderInterpolation(float Alpha);

    /** 현재 보간 Alpha 값 반환 */
    float GetInterpolationAlpha() const { return AccumulatedTime / FixedTimestep; }

    // ═══════════════════════════════════════════════════════════════════════
    // 중력 설정
    // ═══════════════════════════════════════════════════════════════════════

    void SetGravity(const physx::PxVec3& InGravity);
    physx::PxVec3 GetGravity() const;

    // ═══════════════════════════════════════════════════════════════════════
    // PhysX 객체 접근 (Physics 모듈 내부용)
    // ═══════════════════════════════════════════════════════════════════════

    physx::PxScene* GetPxScene() const { return PScene; }
    physx::PxMaterial* GetDefaultMaterial() const { return DefaultMaterial; }

    // ═══════════════════════════════════════════════════════════════════════
    // 통계
    // ═══════════════════════════════════════════════════════════════════════

    uint32 GetNumActors(physx::PxActorTypeFlags Types) const;

private:
    bool CreateScene(UWorld* InOwningWorld);

    // PhysX 객체
    physx::PxScene* PScene = nullptr;
    physx::PxDefaultCpuDispatcher* CpuDispatcher = nullptr;
    physx::PxMaterial* DefaultMaterial = nullptr;
    FPhysicsEventCallback* EventCallback = nullptr;

    // 소유자
    FPhysScene* OwnerScene = nullptr;

    // 상태
    bool bInitialized = false;
    bool bIsSimulating = false;

    // ═══════════════════════════════════════════════════════════════════════
    // 비동기 시뮬레이션 상태
    // ═══════════════════════════════════════════════════════════════════════

    /** 비동기 시뮬레이션 모드 활성화 */
    bool bAsyncSimulation = true;

    /** simulate() 호출 후 fetchResults() 대기 중 */
    bool bSimulationPending = false;

    // 설정
    int32 NumPhysxThreads = 0;

    // 최적 워커 스레드 수 계산
    static int32 CalculateOptimalThreadCount();
    static constexpr float DefaultStaticFriction = 0.5f;
    static constexpr float DefaultDynamicFriction = 0.5f;
    static constexpr float DefaultRestitution = 0.6f;

    // ═══════════════════════════════════════════════════════════════════════
    // Fixed Timestep 설정
    // ═══════════════════════════════════════════════════════════════════════

    /** 물리 시뮬레이션 고정 시간 간격 (1/60초 = 약 16.67ms) */
    static constexpr float FixedTimestep = 1.0f / 60.0f;

    /** 한 프레임에서 최대 허용 서브스텝 수 (무한 루프 방지) */
    static constexpr int32 MaxSubsteps = 8;

    /** 누적된 시간 (Fixed Timestep에 도달할 때까지 누적) */
    float AccumulatedTime = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════
    // 지연 명령 큐 (시뮬레이션 중 Actor 추가/제거 요청 지연 처리)
    // ═══════════════════════════════════════════════════════════════════════

    /** 지연 명령 타입 */
    struct FPhysicsCommand
    {
        enum class EType : uint8
        {
            AddActor,
            RemoveActor
        };

        EType Type;
        physx::PxActor* Actor;
    };

    /** 시뮬레이션 중 대기 중인 명령 큐 */
    TArray<FPhysicsCommand> PendingCommands;

    /** 지연된 명령 처리 */
    void ProcessPendingCommands();

public:
    // ═══════════════════════════════════════════════════════════════════════
    // Actor 추가/제거 (스레드 안전)
    // ═══════════════════════════════════════════════════════════════════════

    /** Actor를 씬에 추가 (시뮬레이션 중이면 지연 처리) */
    void AddActor(physx::PxActor* Actor);

    /** Actor를 씬에서 제거 (시뮬레이션 중이면 지연 처리) */
    void RemoveActor(physx::PxActor* Actor);
};
