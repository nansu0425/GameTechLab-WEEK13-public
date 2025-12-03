#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// PhysScene.h
// 물리 씬 관리자 (World당 하나)
// ─────────────────────────────────────────────────────────────────────────────
//
// PIMPL 패턴으로 PhysX 의존성을 구현부에 숨김.
// 이 헤더를 include하는 코드는 PhysX 헤더가 필요 없음.
// ─────────────────────────────────────────────────────────────────────────────

#include "Vector.h"
#include "UEContainer.h"
#include <memory>
#include <cstdint>

// Forward declarations to avoid leaking PhysX headers
namespace physx
{
    class PxScene;
    class PxMaterial;
}

// 전방 선언 (PhysX 의존성 없음)
class UWorld;
class UVehicleMovementComponent;
class FPhysSceneImpl;  // PIMPL - 구현 숨김

/**
 * @brief 물리 씬 관리자 (World당 하나)
 *
 * 언리얼 엔진의 FPhysScene과 유사한 역할.
 * PIMPL 패턴으로 PhysX 의존성을 구현부에 숨김.
 */
class FPhysScene
{
public:
    FPhysScene();
    ~FPhysScene();

    // 복사 금지 (unique_ptr 멤버)
    FPhysScene(const FPhysScene&) = delete;
    FPhysScene& operator=(const FPhysScene&) = delete;

    // 이동 허용
    FPhysScene(FPhysScene&&) noexcept;
    FPhysScene& operator=(FPhysScene&&) noexcept;

    // ═══════════════════════════════════════════════════════════════════════
    // 생명주기
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 물리 씬 초기화
     * @param InOwningWorld 소유 월드
     */
    void InitPhysScene(UWorld* InOwningWorld);

    /**
     * @brief 물리 씬 종료 및 리소스 해제
     */
    void TermPhysScene();

    /**
     * @brief 초기화 여부 확인
     */
    bool IsInitialized() const;

    // ═══════════════════════════════════════════════════════════════════════
    // 시뮬레이션
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 프레임 시작 (Pre-simulation 작업)
     */
    void StartFrame();

    /**
     * @brief 물리 시뮬레이션 스텝 실행
     * @param DeltaSeconds 프레임 델타 시간 (초)
     */
    void Tick(float DeltaSeconds);

    /**
     * @brief 시뮬레이션 결과 동기화 (Post-simulation 작업)
     */
    void EndFrame();

    /**
     * @brief 시뮬레이션 중 여부 확인
     */
    bool IsSimulating() const;

    // ═══════════════════════════════════════════════════════════════════════
    // 씬 설정
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 중력 설정 (Mundi 좌표계 - Z-Up)
     * @param InGravity 중력 벡터 (예: FVector(0, 0, -981.0f) for cm/s²)
     */
    void SetGravity(const FVector& InGravity);

    /**
     * @brief 현재 중력 값 반환 (Mundi 좌표계)
     */
    FVector GetGravity() const;

    // ═══════════════════════════════════════════════════════════════════════
    // 접근자
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 소유 월드 반환
     */
    UWorld* GetOwningWorld() const;

    // ═══════════════════════════════════════════════════════════════════════
    // PhysX 저수준 객체 접근 (PhysX 의존 코드용)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 내부 PxScene 포인터 반환 (PhysX 기반 시스템에서 사용)
     * @note PhysX 좌표계(+Y Up, -Z Forward) 주의
     */
    physx::PxScene* GetPxScene() const;

    /**
     * @brief 기본 PhysX Material 반환
     */
    physx::PxMaterial* GetDefaultMaterial() const;

    /**
     * @brief PVD 클라이언트 설정 갱신 (PVD 재연결 후 호출)
     */
    void RefreshPvdClient();

    /**
     * @brief 내부 구현 접근 (Physics 모듈 내부에서만 사용)
     *
     * 주의: FPhysSceneImpl은 PhysX 의존성이 있으므로
     *       외부 모듈에서 접근 시 PhysX 헤더 include 필요
     */
    FPhysSceneImpl* GetImpl() const { return Impl.get(); }

    // ═══════════════════════════════════════════════════════════════════════
    // 통계
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief 물리 씬 통계 정보
     */
    struct FPhysSceneStats
    {
        uint32 NumActiveActors = 0;
        uint32 NumStaticActors = 0;
        uint32 NumDynamicActors = 0;
    };

    /**
     * @brief 현재 씬 통계 반환
     */
    FPhysSceneStats GetStats() const;

    // ═══════════════════════════════════════════════════════════════════════
    // Vehicle 컴포넌트 레지스트리
    // ═══════════════════════════════════════════════════════════════════════

    /** VehicleMovementComponent 등록 */
    void RegisterVehicleComponent(UVehicleMovementComponent* InComponent);

    /** VehicleMovementComponent 등록 해제 */
    void UnregisterVehicleComponent(UVehicleMovementComponent* InComponent);

    /** 등록된 VehicleMovementComponent 목록 반환 (무효 포인터 정리 후) */
    TArray<TWeakObjectPtr<UVehicleMovementComponent>> GetRegisteredVehicleComponents() const;

private:
    std::unique_ptr<FPhysSceneImpl> Impl;  // PIMPL - PhysX 구현 숨김
    UWorld* OwningWorld = nullptr;

    /** Vehicle 레이캐스트 연속 스킵 카운터 (씬별 추적) */
    static constexpr uint8 MaxSkipCount = 3; // 레이캐스트 최대 연속 스킵 제한
    uint8 VehicleRaycastSkipCount = 0;
};
