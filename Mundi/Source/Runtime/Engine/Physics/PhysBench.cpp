#include "pch.h"
#include "PhysBench.h"

#include <PxPhysicsAPI.h>

#include "PhysScene.h"
#include "PhysicsStats.h"
#include "PhysicsSceneLock.h"
#include "BodyInstance.h"
#include "PhysBoxActor.h"
#include "PhysGroundActor.h"
#include "BoxComponent.h"
#include "StaticMeshComponent.h"
#include "PlatformTime.h"
#include <cstring>
#include <cstdarg>
#include <cstdio>

using namespace physx;

namespace
{
    const char* DefaultCsvPath = "Data/PhysBench/physbench.csv";

    // 축1 스윕 축 값
    const int32 Axis1Threads[] = { 0, 1, 2, 4, 8, 16, 27 };
    const int32 Axis1Bodies[] = { 500, 2000, 8000, 16000 };

    // 축2: 겹침 이득이 스레드 수에 의존하지 않는지 보려고 두 값에서 측정한다.
    // 4는 축1 예비 측정의 최적점, 27은 CalculateOptimalThreadCount() 가 고르는 값
    const int32 Axis2Threads[] = { 4, 27 };
    const int32 Axis2Bodies[] = { 1000, 2000, 4000, 8000, 12000 };
}

namespace
{
    // UE_LOG 는 인게임 콘솔로만 나가므로, 자동 실행 진단용으로 stdout 에도 남긴다
    void BenchLog(const char* Format, ...)
    {
        char Buf[512];
        va_list Args;
        va_start(Args, Format);
        vsnprintf(Buf, sizeof(Buf), Format, Args);
        va_end(Args);

        puts(Buf);
        fflush(stdout);
        UE_LOG("%s", Buf);
    }
}

FPhysBench& FPhysBench::Get()
{
    static FPhysBench Instance;
    return Instance;
}

const FPhysBenchConfig* FPhysBench::GetActiveConfig() const
{
    return bHasActive ? &Active : nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// 큐 구성
// ═══════════════════════════════════════════════════════════════════════════

void FPhysBench::QueueOne(const FPhysBenchConfig& Config)
{
    Queue.Add(Config);
}

void FPhysBench::QueueAxis1(int32 Repeat)
{
    if (Repeat < 1)
    {
        Repeat = 1;
    }

    for (int32 R = 0; R < Repeat; ++R)
    {
        for (int32 BodyCount : Axis1Bodies)
        {
            for (int32 Threads : Axis1Threads)
            {
                FPhysBenchConfig Config;
                // 축1은 동기 모드로 측정한다. 비동기에서는 fetchResults() 가
                // 남은 대기 시간만 재므로 solver 벽시계 시간이 잡히지 않는다
                Config.bAsync = false;
                Config.ThreadOverride = Threads;
                Config.BodyCount = BodyCount;
                Config.WarmupFrames = 200;
                Config.RecordFrames = 600;
                Config.RepeatIndex = R;
                Queue.Add(Config);
            }
        }
    }
}

void FPhysBench::QueueAxis2(int32 Repeat, int32 OnlyBodyCount)
{
    if (Repeat < 1)
    {
        Repeat = 1;
    }

    for (int32 R = 0; R < Repeat; ++R)
    {
        for (int32 BodyCount : Axis2Bodies)
        {
            // body 수마다 프로세스를 나눠 돌릴 수 있게 한다
            if (OnlyBodyCount > 0 && BodyCount != OnlyBodyCount)
            {
                continue;
            }

            for (int32 Threads : Axis2Threads)
            {
                for (int32 AsyncMode = 0; AsyncMode < 2; ++AsyncMode)
                {
                    FPhysBenchConfig Config;
                    Config.bAsync = (AsyncMode != 0);
                    Config.ThreadOverride = Threads;
                    Config.BodyCount = BodyCount;
                    Config.WarmupFrames = 150;
                    Config.RecordFrames = 400;
                    Config.RepeatIndex = R;
                    Queue.Add(Config);
                }
            }
        }
    }
}

void FPhysBench::ClearQueue()
{
    Queue.Empty();
    QueueCursor = 0;
}

FString FPhysBench::GetStatus() const
{
    const char* StateName =
        State == EState::Idle ? "Idle" :
        State == EState::StartingPie ? "StartingPie" :
        State == EState::Running ? "Running" : "EndingPie";

    char Buf[256];
    sprintf_s(Buf, "PHYSBENCH %s | run %d/%d | frame %d (recorded %d) | samples %d",
        StateName, QueueCursor, static_cast<int32>(Queue.Num()),
        FrameCounter, RecordedCounter, static_cast<int32>(Samples.Num()));

    return FString(Buf);
}

// ═══════════════════════════════════════════════════════════════════════════
// 실행 제어
// ═══════════════════════════════════════════════════════════════════════════

void FPhysBench::ParseCommandLine(const char* CmdLine)
{
    if (CmdLine == nullptr || CmdLine[0] == 0)
    {
        return;
    }

    FString Line(CmdLine);

    const bool bAxis1 = Line.find("-physbench=axis1") != FString::npos;
    const bool bAxis2 = Line.find("-physbench=axis2") != FString::npos;
    const bool bSmoke = Line.find("-physbench=smoke") != FString::npos;
    if (!bAxis1 && !bAxis2 && !bSmoke)
    {
        return;
    }

    auto ReadInt = [&Line](const char* Key, int32 Fallback) -> int32
    {
        const size_t Pos = Line.find(Key);
        if (Pos == FString::npos)
        {
            return Fallback;
        }
        return std::atoi(Line.c_str() + Pos + std::strlen(Key));
    };

    AutoCsvPath = DefaultCsvPath;
    const size_t CsvPos = Line.find("-csv=");
    if (CsvPos != FString::npos)
    {
        FString Rest = Line.substr(CsvPos + 5);
        const size_t Stop = Rest.find_first_of(" \t");
        AutoCsvPath = (Stop == FString::npos) ? Rest : Rest.substr(0, Stop);

        // lpCmdLine 은 호출자가 붙인 따옴표를 그대로 넘긴다
        while (!AutoCsvPath.empty() &&
               (AutoCsvPath.back() == '"' || AutoCsvPath.back() == '\'' || AutoCsvPath.back() == '\r'))
        {
            AutoCsvPath.pop_back();
        }
        if (!AutoCsvPath.empty() && (AutoCsvPath.front() == '"' || AutoCsvPath.front() == '\''))
        {
            AutoCsvPath.erase(AutoCsvPath.begin());
        }
    }

    if (bSmoke)
    {
        // 하네스 검증용 최소 구성: 2 run
        FPhysBenchConfig Config;
        Config.bAsync = ReadInt("-async=", 1) != 0;
        Config.BodyCount = ReadInt("-bodies=", 200);
        Config.WarmupFrames = ReadInt("-warmup=", 60);
        Config.RecordFrames = ReadInt("-frames=", 120);

        const int32 SmokeThreads[] = { 0, 1, 4, 16, 27 };
        for (int32 Threads : SmokeThreads)
        {
            Config.ThreadOverride = Threads;
            QueueOne(Config);
        }
    }
    else
    {
        if (bAxis2)
        {
            QueueAxis2(ReadInt("-repeat=", 1), ReadInt("-bodies=", 0));
        }
        else
        {
            QueueAxis1(ReadInt("-repeat=", 1));
        }

        const int32 Warmup = ReadInt("-warmup=", -1);
        const int32 Frames = ReadInt("-frames=", -1);
        for (FPhysBenchConfig& Config : Queue)
        {
            if (Warmup >= 0) Config.WarmupFrames = Warmup;
            if (Frames > 0)  Config.RecordFrames = Frames;
        }
    }

    bAutoRun = true;
    bAutoStartPending = true;

    BenchLog("[PhysBench] 커맨드라인 자동 실행 예약: runs=%d csv=%s",
        static_cast<int32>(Queue.Num()), AutoCsvPath.c_str());
}

bool FPhysBench::Start(const FString& InCsvPath)
{
    if (Queue.Num() == 0)
    {
        BenchLog("[PhysBench] 큐가 비어 있습니다. PHYSBENCH AXIS1 먼저 실행하세요.");
        return false;
    }

    if (State != EState::Idle)
    {
        BenchLog("[PhysBench] 이미 실행 중입니다.");
        return false;
    }

    CsvPath = InCsvPath.empty() ? FString(DefaultCsvPath) : InCsvPath;
    QueueCursor = 0;
    Samples.Empty();
    SampleConfigs.Empty();
    bCsvHeaderWritten = false;
    TotalRowsWritten = 0;

#ifdef _EDITOR
    if (GEngine.IsPIEActive())
    {
        // PIE가 이미 떠 있으면 먼저 내린 뒤 첫 run 시작
        State = EState::EndingPie;
        return true;
    }
#endif

    BenchLog("[PhysBench] Start: queue=%d csv=%s", static_cast<int32>(Queue.Num()), CsvPath.c_str());
    BeginRun();
    return true;
}

void FPhysBench::Abort()
{
    if (State == EState::Idle)
    {
        return;
    }

    FlushCsv();
    Bodies.Empty();
    bHasActive = false;
    bSceneSpawned = false;
    State = EState::Idle;
    BenchLog("[PhysBench] 중단했습니다.");
}

void FPhysBench::BeginRun()
{
    if (QueueCursor >= static_cast<int32>(Queue.Num()))
    {
        FlushCsv();
        bHasActive = false;
        State = EState::Idle;
        BenchLog("[PhysBench] 전체 완료. rows=%d", TotalRowsWritten);

        if (bAutoRun)
        {
            bAutoRun = false;
            PostQuitMessage(0);
        }
        return;
    }

    Active = Queue[QueueCursor];
    SampleConfigs.Add(Active);
    ++QueueCursor;

    bHasActive = true;
    bSceneSpawned = false;
    FrameCounter = 0;
    RecordedCounter = 0;
    LastFrameCycles = 0;
    Bodies.Empty();

    BenchLog("[PhysBench] run %d/%d 시작: async=%d threads=%d bodies=%d rep=%d",
        QueueCursor, static_cast<int32>(Queue.Num()),
        Active.bAsync ? 1 : 0, Active.ThreadOverride, Active.BodyCount, Active.RepeatIndex);

    State = EState::StartingPie;
}

void FPhysBench::FinishRun()
{
    FlushCsv();
    Bodies.Empty();
    bSceneSpawned = false;
    bHasActive = false;
    State = EState::EndingPie;
}

// ═══════════════════════════════════════════════════════════════════════════
// 프레임 훅
// ═══════════════════════════════════════════════════════════════════════════

void FPhysBench::TickDriver()
{
#ifdef _EDITOR
    if (bAutoStartPending)
    {
        bAutoStartPending = false;
        Start(AutoCsvPath);
    }

    switch (State)
    {
    case EState::Idle:
    case EState::Running:
        return;

    case EState::StartingPie:
        if (!GEngine.IsPIEActive())
        {
            GEngine.StartPIE();
        }
        if (GEngine.IsPIEActive())
        {
            BenchLog("[PhysBench] PIE 진입 완료");
            State = EState::Running;
        }
        else
        {
            BenchLog("[PhysBench] PIE 진입 실패");
        }
        return;

    case EState::EndingPie:
        if (GEngine.IsPIEActive())
        {
            // 지연 종료 — 다음 MainLoop 반복에서 실제로 내려간다
            GEngine.EndPIE();
            return;
        }
        BeginRun();
        return;
    }
#endif
}

void FPhysBench::PreWorldTick(UWorld* World)
{
    if (State != EState::Running || World == nullptr)
    {
        return;
    }

#ifdef _EDITOR
    if (!GEngine.IsPIEActive() || World != GWorld)
    {
        return;
    }
#endif

    if (!bSceneSpawned)
    {
        SpawnScene(World);
        bSceneSpawned = true;
        return;
    }

    RecycleFallen(World);
}

void FPhysBench::PostWorldTick(UWorld* World)
{
    if (State != EState::Running || !bSceneSpawned || World == nullptr)
    {
        return;
    }

#ifdef _EDITOR
    if (!GEngine.IsPIEActive() || World != GWorld)
    {
        return;
    }
#endif

    const uint64 Now = FPlatformTime::Cycles64();
    const double FrameMs = (LastFrameCycles != 0)
        ? FPlatformTime::ToMilliseconds(Now - LastFrameCycles)
        : 0.0;
    LastFrameCycles = Now;

    ++FrameCounter;
    if (FrameCounter <= Active.WarmupFrames)
    {
        return;
    }

    const FPhysicsStats& Stats = FPhysicsStatManager::GetInstance().GetStats();

    FPhysBenchSample Row;
    Row.RunIndex = QueueCursor - 1;
    Row.FrameIndex = FrameCounter;
    Row.FrameMs = FrameMs;
    Row.SimulateMs = Stats.SimulateTimeMs;
    Row.FetchMs = Stats.FetchResultsTimeMs;
    Row.InterpMs = Stats.InterpolationUpdateTimeMs;
    Row.TotalMs = Stats.TotalPhysicsTimeMs;
    Row.ActorTickMs = Stats.ActorTickTimeMs;
    Row.Substeps = Stats.SubstepCount;
    Row.ActiveCount = Stats.ActiveActorCount;
    Row.DynamicCount = Stats.DynamicActorCount;
    Row.StaticCount = Stats.StaticActorCount;
    Row.ContactCount = Stats.ContactEventCount;
    Row.ThreadsActual = Stats.PhysicsThreadCount;

    Samples.Add(Row);
    ++RecordedCounter;

    if (RecordedCounter >= Active.RecordFrames)
    {
        FinishRun();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 씬 구성
// ═══════════════════════════════════════════════════════════════════════════

void FPhysBench::SpawnScene(UWorld* World)
{
    const int32 N = FMath::Max(Active.BodyCount, 1);

    // N개를 정육면체 격자로 배치
    int32 Side = static_cast<int32>(std::ceil(std::cbrt(static_cast<double>(N))));
    Side = FMath::Max(Side, 1);

    const float Spacing = 1.4f;
    GroundZ = 50.0f;
    SpawnHalfSpan = Side * Spacing * 0.5f;
    SpawnTopZ = GroundZ + 2.0f;

    const float GroundHalf = SpawnHalfSpan + 10.0f;

    // 바닥 — 기존 씬 지오메트리와 겹치지 않도록 Z=50에 새로 만든다
    AActor* GroundActor = World->SpawnActor(
        APhysGroundActor::StaticClass(),
        FTransform(FVector(0.0f, 0.0f, GroundZ), FQuat(0, 0, 0, 1), FVector::One()));

    if (APhysGroundActor* Ground = Cast<APhysGroundActor>(GroundActor))
    {
        Ground->GetMeshComponent()->SetWorldScale(FVector(GroundHalf * 2.0f, GroundHalf * 2.0f, 0.2f));
        Ground->GetMeshComponent()->SetVisibility(false);
        // BoxExtent 0.5 → 월드 half-extent = 0.5 * 부모 스케일 = GroundHalf
        Ground->GetBoxComponent()->SetBoxExtent(FVector(0.5f, 0.5f, 0.5f), true);
    }

    // 바디 격자
    Bodies.Empty();
    for (int32 i = 0; i < N; ++i)
    {
        const int32 Ix = i % Side;
        const int32 Iy = (i / Side) % Side;
        const int32 Iz = i / (Side * Side);

        const FVector Loc(
            (Ix - (Side - 1) * 0.5f) * Spacing,
            (Iy - (Side - 1) * 0.5f) * Spacing,
            SpawnTopZ + Iz * Spacing);

        AActor* Spawned = World->SpawnActor(
            APhysBoxActor::StaticClass(),
            FTransform(Loc, FQuat(0, 0, 0, 1), FVector::One()));

        if (APhysBoxActor* Box = Cast<APhysBoxActor>(Spawned))
        {
            // 렌더 비용이 프레임을 지배하면 물리 측정이 묻힌다. 물리와 Actor Tick 은 그대로 돈다
            if (UStaticMeshComponent* Mesh = Box->GetMeshComponent())
            {
                Mesh->SetVisibility(false);
            }
            Bodies.Add(Box);
        }
    }

    // sleep 비활성화 — 방치하면 active 수가 시간에 따라 줄어 측정값이 계속 떨어진다
    FPhysScene* Scene = World->GetPhysScene();
    PxScene* PScene = Scene ? Scene->GetPxScene() : nullptr;
    if (PScene)
    {
        SCOPED_SCENE_WRITE_LOCK(PScene);
        for (APhysBoxActor* Box : Bodies)
        {
            if (!Box || !Box->GetBoxComponent())
            {
                continue;
            }
            PxRigidDynamic* Dynamic = Box->GetBoxComponent()->GetBodyInstanceRef().GetPxRigidDynamic();
            if (Dynamic)
            {
                Dynamic->setSleepThreshold(0.0f);
                Dynamic->wakeUp();
            }
        }
    }

    BenchLog("[PhysBench] 스폰 완료: bodies=%d side=%d groundHalf=%.1f",
        static_cast<int32>(Bodies.Num()), Side, GroundHalf);
}

void FPhysBench::RecycleFallen(UWorld* World)
{
    const float KillZ = GroundZ - 20.0f;
    const float ReturnZ = SpawnTopZ + SpawnHalfSpan * 2.0f;

    for (APhysBoxActor* Box : Bodies)
    {
        if (!Box || !Box->GetBoxComponent())
        {
            continue;
        }

        // 위치는 APhysBoxActor::Tick 이 물리에서 동기화한 값 — PhysX 재조회 없음
        const FVector Loc = Box->GetActorLocation();
        if (Loc.Z >= KillZ)
        {
            continue;
        }

        const FVector NewLoc(
            FMath::Clamp(Loc.X, -SpawnHalfSpan, SpawnHalfSpan),
            FMath::Clamp(Loc.Y, -SpawnHalfSpan, SpawnHalfSpan),
            ReturnZ);

        Box->GetBoxComponent()->GetBodyInstanceRef().SetWorldTransform(
            FTransform(NewLoc, FQuat(0, 0, 0, 1), FVector::One()), true);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CSV
// ═══════════════════════════════════════════════════════════════════════════

bool FPhysBench::FlushCsv()
{
    if (Samples.Num() == 0)
    {
        return false;
    }

    try
    {
        std::filesystem::path Path(CsvPath);
        if (Path.has_parent_path())
        {
            std::filesystem::create_directories(Path.parent_path());
        }
    }
    catch (const std::exception&)
    {
        // 디렉터리 생성 실패는 ofstream 열기 실패로 드러난다
    }

    // run 이 끝날 때마다 이어 쓴다. 스윕이 중간에 죽어도 그때까지의 결과는 남는다
    const bool bFirst = !bCsvHeaderWritten;
    std::ofstream Out(CsvPath,
        bFirst ? (std::ios::out | std::ios::trunc) : (std::ios::out | std::ios::app));
    if (!Out.is_open())
    {
        BenchLog("[PhysBench] CSV 열기 실패: %s", CsvPath.c_str());
        return false;
    }

    if (bFirst)
    {
        Out << "run,async,threads_cfg,bodies_cfg,repeat,"
            << "frame,frame_ms,sim_ms,fetch_ms,interp_ms,total_ms,actor_tick_ms,"
            << "substeps,active,dyn,static,contacts,threads_actual\n";
        bCsvHeaderWritten = true;
    }

    for (const FPhysBenchSample& S : Samples)
    {
        const FPhysBenchConfig& Cfg = SampleConfigs[S.RunIndex];

        Out << S.RunIndex << ','
            << (Cfg.bAsync ? 1 : 0) << ','
            << Cfg.ThreadOverride << ','
            << Cfg.BodyCount << ','
            << Cfg.RepeatIndex << ','
            << S.FrameIndex << ','
            << S.FrameMs << ','
            << S.SimulateMs << ','
            << S.FetchMs << ','
            << S.InterpMs << ','
            << S.TotalMs << ','
            << S.ActorTickMs << ','
            << S.Substeps << ','
            << S.ActiveCount << ','
            << S.DynamicCount << ','
            << S.StaticCount << ','
            << S.ContactCount << ','
            << S.ThreadsActual << '\n';
    }

    Out.close();
    BenchLog("[PhysBench] CSV 추가: %s (+%d rows)", CsvPath.c_str(), static_cast<int32>(Samples.Num()));

    TotalRowsWritten += static_cast<int32>(Samples.Num());

    // 메모리에 쌓지 않는다 — 16000 body 스윕에서 시스템 메모리가 문제가 된다
    Samples.Empty();
    return true;
}
