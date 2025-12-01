#pragma once

#include "UEContainer.h"

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsStats.h
// 물리 시뮬레이션 성능 측정을 위한 통계 구조체 및 매니저
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 물리 시뮬레이션 통계 구조체
 *
 * 프레임당 물리 시뮬레이션 성능 및 상태 정보를 저장
 */
struct FPhysicsStats
{
	// ═══════════════════════════════════════════════════════════════════════
	// 타이밍 (밀리초)
	// ═══════════════════════════════════════════════════════════════════════

	/** PScene->simulate() 호출 시간 */
	double SimulateTimeMs = 0.0;

	/** PScene->fetchResults() 호출 시간 */
	double FetchResultsTimeMs = 0.0;

	/** 전체 물리 프레임 시간 (StartFrame ~ EndFrame) */
	double TotalPhysicsTimeMs = 0.0;

	/** 렌더 보간 업데이트 시간 */
	double InterpolationUpdateTimeMs = 0.0;

	/** Component 동기화 시간 */
	double SyncToComponentTimeMs = 0.0;

	// ═══════════════════════════════════════════════════════════════════════
	// Actor 카운트
	// ═══════════════════════════════════════════════════════════════════════

	/** 현재 활성 Actor 수 */
	uint32 ActiveActorCount = 0;

	/** Dynamic body 수 */
	uint32 DynamicActorCount = 0;

	/** Static body 수 */
	uint32 StaticActorCount = 0;

	// ═══════════════════════════════════════════════════════════════════════
	// 이벤트 카운트 (프레임당)
	// ═══════════════════════════════════════════════════════════════════════

	/** 프레임당 Contact 이벤트 수 */
	uint32 ContactEventCount = 0;

	/** 프레임당 Trigger 이벤트 수 */
	uint32 TriggerEventCount = 0;

	// ═══════════════════════════════════════════════════════════════════════
	// 동기화/프레임 상태
	// ═══════════════════════════════════════════════════════════════════════

	/** 지연된 Add/Remove 명령 수 */
	uint32 PendingCommandCount = 0;

	/** 프레임당 물리 substep 횟수 */
	uint32 SubstepCount = 0;

	/** 누적시간/FixedTimestep 비율 (보간 알파) */
	float AccumulatedTimeRatio = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════
	// 시스템 정보
	// ═══════════════════════════════════════════════════════════════════════

	/** PhysX CPU 디스패처 스레드 수 */
	uint32 PhysicsThreadCount = 0;

	// ═══════════════════════════════════════════════════════════════════════
	// 메서드
	// ═══════════════════════════════════════════════════════════════════════

	/** 모든 통계를 0으로 리셋 */
	void Reset()
	{
		SimulateTimeMs = 0.0;
		FetchResultsTimeMs = 0.0;
		TotalPhysicsTimeMs = 0.0;
		InterpolationUpdateTimeMs = 0.0;
		SyncToComponentTimeMs = 0.0;

		ActiveActorCount = 0;
		DynamicActorCount = 0;
		StaticActorCount = 0;

		ContactEventCount = 0;
		TriggerEventCount = 0;

		PendingCommandCount = 0;
		SubstepCount = 0;
		AccumulatedTimeRatio = 0.0f;
	}

	/** 전체 오버헤드 시간 계산 (simulate/fetch 제외) */
	double GetOverheadTimeMs() const
	{
		return TotalPhysicsTimeMs - SimulateTimeMs - FetchResultsTimeMs;
	}
};

/**
 * @brief 물리 통계 전역 매니저 (싱글톤)
 *
 * UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
 */
class FPhysicsStatManager
{
public:
	static FPhysicsStatManager& GetInstance()
	{
		static FPhysicsStatManager Instance;
		return Instance;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 통계 접근
	// ═══════════════════════════════════════════════════════════════════════

	/** 전체 통계 업데이트 */
	void UpdateStats(const FPhysicsStats& InStats)
	{
		CurrentStats = InStats;
	}

	/** 통계 조회 */
	const FPhysicsStats& GetStats() const
	{
		return CurrentStats;
	}

	/** 프레임 시작 시 통계 리셋 */
	void ResetFrameStats()
	{
		// 이벤트 카운트만 리셋 (타이밍은 측정 후 덮어씀)
		CurrentStats.ContactEventCount = 0;
		CurrentStats.TriggerEventCount = 0;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 개별 stat 업데이트 헬퍼
	// ═══════════════════════════════════════════════════════════════════════

	void RecordSimulateTime(double Ms) { CurrentStats.SimulateTimeMs = Ms; }
	void RecordFetchResultsTime(double Ms) { CurrentStats.FetchResultsTimeMs = Ms; }
	void RecordTotalPhysicsTime(double Ms) { CurrentStats.TotalPhysicsTimeMs = Ms; }
	void RecordInterpolationUpdateTime(double Ms) { CurrentStats.InterpolationUpdateTimeMs = Ms; }
	void RecordSyncToComponentTime(double Ms) { CurrentStats.SyncToComponentTimeMs = Ms; }

	void SetActiveActorCount(uint32 Count) { CurrentStats.ActiveActorCount = Count; }
	void SetDynamicActorCount(uint32 Count) { CurrentStats.DynamicActorCount = Count; }
	void SetStaticActorCount(uint32 Count) { CurrentStats.StaticActorCount = Count; }

	void IncrementContactEvents() { ++CurrentStats.ContactEventCount; }
	void IncrementTriggerEvents() { ++CurrentStats.TriggerEventCount; }

	void SetPendingCommandCount(uint32 Count) { CurrentStats.PendingCommandCount = Count; }
	void SetSubstepCount(uint32 Count) { CurrentStats.SubstepCount = Count; }
	void SetAccumulatedTimeRatio(float Ratio) { CurrentStats.AccumulatedTimeRatio = Ratio; }

	void SetPhysicsThreadCount(uint32 Count) { CurrentStats.PhysicsThreadCount = Count; }

	// 프레임 타이밍 측정용
	void BeginFrameTiming(uint64 StartCycles) { FrameStartCycles = StartCycles; }
	void EndFrameTiming(uint64 EndCycles, double (*ToMs)(uint64))
	{
		CurrentStats.TotalPhysicsTimeMs = ToMs(EndCycles - FrameStartCycles);
	}

private:
	FPhysicsStatManager() = default;
	~FPhysicsStatManager() = default;
	FPhysicsStatManager(const FPhysicsStatManager&) = delete;
	FPhysicsStatManager& operator=(const FPhysicsStatManager&) = delete;

	FPhysicsStats CurrentStats;
	uint64 FrameStartCycles = 0;
};
