#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// PhysBench.h
// 물리 시뮬레이션 프로파일링 하네스
// ─────────────────────────────────────────────────────────────────────────────
//
// config 큐를 순서대로 실행한다. 1 run = PIE 진입 → 바디 스폰 → warmup →
// 프레임 기록 → PIE 종료. 기록은 메모리에 쌓고 run 종료 시 CSV로 1회 flush 한다.
// 프레임마다 파일에 쓰면 그 I/O가 측정 대상인 프레임 시간에 섞인다.
// ─────────────────────────────────────────────────────────────────────────────

#include "UEContainer.h"
#include "Vector.h"

class UWorld;
class APhysBoxActor;

/** 프로파일링 설정 1건 */
struct FPhysBenchConfig
{
    /** 비동기 시뮬레이션 사용 여부 (FPhysSceneImpl::bAsyncSimulation) */
    bool bAsync = true;

    /** PhysX CPU 디스패처 워커 스레드 수. -1이면 CalculateOptimalThreadCount() */
    int32 ThreadOverride = -1;

    /** 스폰할 dynamic body 수 */
    int32 BodyCount = 500;

    /** 기록 전 버릴 프레임 수 */
    int32 WarmupFrames = 300;

    /** 기록할 프레임 수 */
    int32 RecordFrames = 1200;

    /** 동일 설정의 반복 회차 (0-based) */
    int32 RepeatIndex = 0;
};

/** CSV 한 줄 */
struct FPhysBenchSample
{
    int32 RunIndex = 0;
    int32 FrameIndex = 0;
    double FrameMs = 0.0;
    double SimulateMs = 0.0;
    double FetchMs = 0.0;
    double InterpMs = 0.0;
    double TotalMs = 0.0;
    double ActorTickMs = 0.0;
    uint32 Substeps = 0;
    uint32 ActiveCount = 0;
    uint32 DynamicCount = 0;
    uint32 StaticCount = 0;
    uint32 ContactCount = 0;
    uint32 ThreadsActual = 0;
};

class FPhysBench
{
public:
    static FPhysBench& Get();

    // ═══════════════════════════════════════════════════════════════════════
    // 설정 주입 — FPhysSceneImpl::CreateScene() 에서 조회
    // ═══════════════════════════════════════════════════════════════════════

    /** 현재 실행 중인 config. 벤치가 돌고 있지 않으면 nullptr */
    const FPhysBenchConfig* GetActiveConfig() const;

    // ═══════════════════════════════════════════════════════════════════════
    // 콘솔 명령
    // ═══════════════════════════════════════════════════════════════════════

    /** 축1(스레드 스케일링) 스윕을 큐에 적재 */
    void QueueAxis1(int32 Repeat);

    /** 축2(비동기 겹침) 스윕을 큐에 적재 */
    void QueueAxis2(int32 Repeat, int32 OnlyBodyCount = 0);

    /** 단일 config 를 큐에 적재 */
    void QueueOne(const FPhysBenchConfig& Config);

    void ClearQueue();

    /** 큐 실행 시작. CsvPath 가 비면 기본 경로 사용 */
    bool Start(const FString& InCsvPath);

    void Abort();

    FString GetStatus() const;

    // ═══════════════════════════════════════════════════════════════════════
    // 프레임 훅
    // ═══════════════════════════════════════════════════════════════════════

    /** 커맨드라인 파싱 후 자동 실행 예약. 예: -physbench=axis1 -repeat=3 -csv=out.csv */
    void ParseCommandLine(const char* CmdLine);

    /** 자동 실행 모드 — 전체 완료 시 앱을 종료한다 */
    bool IsAutoRun() const { return bAutoRun; }

    /** UEditorEngine::MainLoop — PIE 진입/종료 구동 */
    void TickDriver();

    /** UWorld::Tick 선두 — 스폰, 리사이클 */
    void PreWorldTick(UWorld* World);

    /** UWorld::Tick 말미 — 샘플 기록, run 종료 판정 */
    void PostWorldTick(UWorld* World);

private:
    FPhysBench() = default;

    enum class EState : uint8
    {
        Idle,
        StartingPie,    // StartPIE() 호출 후 PIE 활성화 대기
        Running,        // 스폰/warmup/기록
        EndingPie,      // EndPIE() 호출 후 PIE 종료 대기
    };

    void BeginRun();
    void FinishRun();
    void SpawnScene(UWorld* World);
    void RecycleFallen(UWorld* World);
    bool FlushCsv();

    EState State = EState::Idle;

    TArray<FPhysBenchConfig> Queue;
    int32 QueueCursor = 0;

    FPhysBenchConfig Active;
    bool bHasActive = false;

    // 현재 run 상태
    bool bSceneSpawned = false;
    int32 FrameCounter = 0;
    int32 RecordedCounter = 0;
    uint64 LastFrameCycles = 0;

    TArray<APhysBoxActor*> Bodies;
    float GroundZ = 0.0f;
    float SpawnTopZ = 0.0f;
    float SpawnHalfSpan = 0.0f;

    // 누적 결과
    TArray<FPhysBenchSample> Samples;
    TArray<FPhysBenchConfig> SampleConfigs;   // RunIndex → config
    FString CsvPath;
    bool bCsvHeaderWritten = false;
    int32 TotalRowsWritten = 0;

    // 커맨드라인 자동 실행
    bool bAutoRun = false;
    bool bAutoStartPending = false;
    FString AutoCsvPath;
};
