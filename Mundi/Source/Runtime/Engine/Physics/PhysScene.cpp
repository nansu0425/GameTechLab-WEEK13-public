#include "pch.h"
#include "PhysScene.h"
#include "PhysSceneImpl.h"
#include "PhysicsCore.h"
#include "PhysicsTypes.h"
#include "PhysicsEventCallback.h"
#include "PhysicsStats.h"
#include "BodyInstance.h"
#include "World.h"
#include "GlobalConsole.h"
#include "PlatformTime.h"
#include "SimpleWheeledVehicleMovementComponent.h"
#include "PhysicsSceneLock.h"
#include "ClothCore.h"
#include <NvCloth/Solver.h>

using namespace physx;

// ═══════════════════════════════════════════════════════════════════════════════
// 충돌 이벤트를 받기 위한 커스텀 필터 셰이더
// ═══════════════════════════════════════════════════════════════════════════════
static PxFilterFlags MundiFilterShader(
    PxFilterObjectAttributes Attributes0,
    PxFilterData FilterData0,
    PxFilterObjectAttributes Attributes1,
    PxFilterData FilterData1,
    PxPairFlags& PairFlags,
    const void* ConstantBlock,
    PxU32 ConstantBlockSize)
{
    // 트리거 처리
    if (PxFilterObjectIsTrigger(Attributes0) || PxFilterObjectIsTrigger(Attributes1))
    {
        PairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    // 기본 충돌 처리 + 이벤트 알림
    PairFlags = PxPairFlag::eCONTACT_DEFAULT
              | PxPairFlag::eNOTIFY_TOUCH_FOUND
              | PxPairFlag::eNOTIFY_TOUCH_PERSISTS
              | PxPairFlag::eNOTIFY_TOUCH_LOST
              | PxPairFlag::eNOTIFY_CONTACT_POINTS;

    return PxFilterFlag::eDEFAULT;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FPhysScene - 공개 래퍼
// ═══════════════════════════════════════════════════════════════════════════════

FPhysScene::FPhysScene()
    : Impl(std::make_unique<FPhysSceneImpl>())
{
}

FPhysScene::~FPhysScene()
{
    TermPhysScene();
}

FPhysScene::FPhysScene(FPhysScene&&) noexcept = default;
FPhysScene& FPhysScene::operator=(FPhysScene&&) noexcept = default;

void FPhysScene::InitPhysScene(UWorld* InOwningWorld)
{
    OwningWorld = InOwningWorld;

    if (!FPhysicsCore::Get().IsInitialized())
    {
        UE_LOG("FPhysScene: FPhysicsCore not initialized");
        return;
    }

    if (!Impl->Initialize(this, InOwningWorld))
    {
        UE_LOG("FPhysScene: Failed to initialize");
        return;
    }

    UE_LOG("FPhysScene: Initialization complete");
}

void FPhysScene::TermPhysScene()
{
    if (Impl)
    {
        Impl->Terminate();
    }
    OwningWorld = nullptr;
}

bool FPhysScene::IsInitialized() const
{
    return Impl && Impl->IsInitialized();
}

void FPhysScene::StartFrame()
{
    // 프레임 시작 시 통계 리셋 및 타이밍 시작
    FPhysicsStatManager& StatManager = FPhysicsStatManager::GetInstance();
    StatManager.ResetFrameStats();
    StatManager.BeginFrameTiming(FPlatformTime::Cycles64());

    // 레이캐스트 수행 시점: 시뮬레이션 중이면 스킵하지만, 연속 스킵이 MaxSkipCount를 넘으면 강제 수행
    bool bShouldForceRaycast = VehicleRaycastSkipCount >= MaxSkipCount;
    if (!IsSimulating() || bShouldForceRaycast)
    {
        physx::PxScene* PxScene = Impl ? Impl->GetPxScene() : nullptr;
        if (PxScene)
        {
            SCOPED_SCENE_READ_LOCK(PxScene);
            TArray<TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>> Vehicles = GetRegisteredVehicleComponents();
            for (TWeakObjectPtr<USimpleWheeledVehicleMovementComponent> VehiclePtr : Vehicles)
            {
                if (VehiclePtr.IsValid())
                {
                    VehiclePtr->PerformSuspensionRaycasts();
                }
            }
        }
        if (bShouldForceRaycast)
        {
			UE_LOG("FPhysScene: Forced vehicle raycasts after %d skipped frames", static_cast<int32>(VehicleRaycastSkipCount));
        }

        VehicleRaycastSkipCount = 0;
    }
    else
    {
        VehicleRaycastSkipCount++;
    }

    if (Impl)
    {
        Impl->StartFrame();
    }
}

void FPhysScene::Tick(float DeltaSeconds)
{
    if (Impl)
    {
        Impl->Simulate(DeltaSeconds);
    }

    // NvCloth simulation (after PhysX)
    if (FClothCore::GetInstance().IsInitialized())
    {
        nv::cloth::Solver* ClothSolver = FClothCore::GetInstance().GetSolver();
        if (ClothSolver)
        {
            int32 NumCloths = ClothSolver->getNumCloths();

            // Debug: Print cloth count on first frame with cloth
            static bool bFirstClothFrame = true;
            if (NumCloths > 0 && bFirstClothFrame)
            {
                printf("[PhysScene] Cloth simulation active: %d cloths in solver\n", NumCloths);
                bFirstClothFrame = false;
            }

            if (NumCloths > 0)
            {
                ClothSolver->beginSimulation(DeltaSeconds);

                // Simulate all chunks (can be parallelized)
                int32 NumChunks = ClothSolver->getSimulationChunkCount();
                for (int32 i = 0; i < NumChunks; ++i)
                {
                    ClothSolver->simulateChunk(i);
                }

                ClothSolver->endSimulation();
            }
        }
    }
}

void FPhysScene::EndFrame()
{
    if (Impl)
    {
        Impl->FetchResults();
    }

    // PhysX 결과가 확정된 후 차량 포즈를 갱신
    TArray<TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>> Vehicles = GetRegisteredVehicleComponents();
    for (TWeakObjectPtr<USimpleWheeledVehicleMovementComponent> VehiclePtr : Vehicles)
    {
        if (VehiclePtr.IsValid())
        {
            VehiclePtr->UpdateVehiclePoseFromPhysX();
        }
    }

    // 프레임 종료 시 총 물리 시간 기록
    FPhysicsStatManager::GetInstance().EndFrameTiming(
        FPlatformTime::Cycles64(),
        FPlatformTime::ToMilliseconds);
}

bool FPhysScene::IsSimulating() const
{
    return Impl && Impl->IsSimulating();
}

void FPhysScene::SetGravity(const FVector& InGravity)
{
    if (Impl)
    {
        // Mundi 좌표계 → PhysX 좌표계 변환
        Impl->SetGravity(PhysicsConversion::ToPxVec3(InGravity));
    }
}

FVector FPhysScene::GetGravity() const
{
    if (Impl)
    {
        // PhysX 좌표계 → Mundi 좌표계 변환
        return PhysicsConversion::ToFVector(Impl->GetGravity());
    }
    return FVector(0, 0, -981.0f);  // 기본 중력 (cm/s²)
}

UWorld* FPhysScene::GetOwningWorld() const
{
    return OwningWorld;
}

physx::PxScene* FPhysScene::GetPxScene() const
{
    return Impl ? Impl->GetPxScene() : nullptr;
}

physx::PxMaterial* FPhysScene::GetDefaultMaterial() const
{
    return Impl ? Impl->GetDefaultMaterial() : nullptr;
}

FPhysScene::FPhysSceneStats FPhysScene::GetStats() const
{
    FPhysSceneStats Stats;
    if (Impl)
    {
        Stats.NumDynamicActors = Impl->GetNumActors(PxActorTypeFlag::eRIGID_DYNAMIC);
        Stats.NumStaticActors = Impl->GetNumActors(PxActorTypeFlag::eRIGID_STATIC);
        Stats.NumActiveActors = Stats.NumDynamicActors;  // TODO: 실제 active 수 계산
    }
    return Stats;
}

void FPhysScene::RegisterVehicleComponent(USimpleWheeledVehicleMovementComponent* InComponent)
{
    if (Impl)
    {
        Impl->RegisterVehicleComponent(InComponent);
    }
}

void FPhysScene::UnregisterVehicleComponent(USimpleWheeledVehicleMovementComponent* InComponent)
{
    if (Impl)
    {
        Impl->UnregisterVehicleComponent(InComponent);
    }
}

TArray<TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>> FPhysScene::GetRegisteredVehicleComponents() const
{
    if (Impl)
    {
        return Impl->GetVehicleComponents();
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════════════════════
// FPhysSceneImpl - PhysX 구현부
// ═══════════════════════════════════════════════════════════════════════════════

FPhysSceneImpl::FPhysSceneImpl()
{
}

FPhysSceneImpl::~FPhysSceneImpl()
{
    Terminate();
}

bool FPhysSceneImpl::Initialize(FPhysScene* InOwnerScene, UWorld* InOwningWorld)
{
    if (bInitialized)
        return true;

    OwnerScene = InOwnerScene;

    if (!CreateScene(InOwningWorld))
        return false;

    // 기본 Material 생성
    DefaultMaterial = FPhysicsCore::Get().CreateMaterial(
        DefaultStaticFriction, DefaultDynamicFriction, DefaultRestitution);

    // 테스트용 Actor 생성 (PVD 확인용)
    // CreateTestActors();

    // 물리 스레드 수 통계 설정
    FPhysicsStatManager::GetInstance().SetPhysicsThreadCount(NumPhysxThreads);

    bInitialized = true;
    return true;
}

void FPhysSceneImpl::Terminate()
{
    if (PScene)
    {
        PScene->release();
        PScene = nullptr;
    }

    if (CpuDispatcher)
    {
        CpuDispatcher->release();
        CpuDispatcher = nullptr;
    }

    // EventCallback 해제
    if (EventCallback)
    {
        delete EventCallback;
        EventCallback = nullptr;
    }

    DefaultMaterial = nullptr;
    bInitialized = false;

    UE_LOG("FPhysSceneImpl: Terminated");
}

bool FPhysSceneImpl::CreateScene(UWorld* InOwningWorld)
{
    FPhysicsCore& Core = FPhysicsCore::Get();
    PxPhysics* Physics = Core.GetPhysics();

    PxSceneDesc SceneDesc(Physics->getTolerancesScale());

    // 중력 설정 (PhysX Y-Up 좌표계)
    SceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

    // 최적 워커 스레드 수 계산 및 CPU Dispatcher 생성
    NumPhysxThreads = CalculateOptimalThreadCount();
    CpuDispatcher = PxDefaultCpuDispatcherCreate(NumPhysxThreads);
    SceneDesc.cpuDispatcher = CpuDispatcher;

    UE_LOG("FPhysSceneImpl: Using %d physics worker threads", NumPhysxThreads);

    // 필터 셰이더 - 커스텀 셰이더로 충돌 이벤트 활성화
    SceneDesc.filterShader = MundiFilterShader;

    // EventCallback 생성 및 등록
    EventCallback = new FPhysicsEventCallback(OwnerScene);
    SceneDesc.simulationEventCallback = EventCallback;

    // Scene 플래그
    SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;  // 활성 Actor 추적
    SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;            // 연속 충돌 감지
    SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;            // 지속적 접촉 다중점

    PScene = Physics->createScene(SceneDesc);
    if (!PScene)
    {
        delete EventCallback;
        EventCallback = nullptr;
        return false;
    }

    UE_LOG("FPhysSceneImpl: EventCallback 등록 완료");

    // PVD 연결
    PxPvd* Pvd = Core.GetPvd();
    if (Pvd && Pvd->isConnected())
    {
        PxPvdSceneClient* PvdClient = PScene->getScenePvdClient();
        if (PvdClient)
        {
            PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
            PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
            PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
        }
    }

    return true;
}

void FPhysSceneImpl::StartFrame()
{
    // 비동기 모드에서는 EndFrame()에서 fetchResults() 처리
    // StartFrame()은 확장을 위해 예약 (Pre-simulation 작업 등)
}

void FPhysSceneImpl::Simulate(float DeltaSeconds)
{
    if (!PScene)
        return;

    FPhysicsStatManager& StatManager = FPhysicsStatManager::GetInstance();

    auto ApplyVehicleInputs = [this](float Step)
    {
        if (Step <= 0.0f)
        {
            return;
        }
        CompactVehicleComponents();
        for (TWeakObjectPtr<USimpleWheeledVehicleMovementComponent> VehiclePtr : VehicleComponents)
        {
            if (VehiclePtr.IsValid())
            {
                VehiclePtr->ApplyInputToPhysX(Step);
            }
        }
    };

    auto SimulateVehicles = [this](float Step)
    {
        if (Step <= 0.0f)
        {
            return;
        }
        CompactVehicleComponents();
        for (TWeakObjectPtr<USimpleWheeledVehicleMovementComponent> VehiclePtr : VehicleComponents)
        {
            if (VehiclePtr.IsValid())
            {
                VehiclePtr->SimulateVehicle(Step);
            }
        }
    };

    // ═══════════════════════════════════════════════════════════════════════
    // 비동기 물리 시뮬레이션 (언리얼 엔진 스타일)
    // ═══════════════════════════════════════════════════════════════════════
    // simulate() 호출 후 fetchResults()를 즉시 호출하지 않고,
    // 다음 프레임 StartFrame() 또는 EndFrame()에서 결과를 수집합니다.
    // 이를 통해 Actor::Tick()이 물리 시뮬레이션과 병렬로 실행될 수 있습니다.

    if (bAsyncSimulation)
    {
        // 비정상적으로 큰 DeltaTime 제한 (예: 디버거 일시정지 후)
        DeltaSeconds = FMath::Min(DeltaSeconds, MaxSubsteps * FixedTimestep);
        AccumulatedTime += DeltaSeconds;

        // Fixed Timestep 도달 시 시뮬레이션 시작
        if (AccumulatedTime >= FixedTimestep)
        {
            // 처리할 스텝 수 계산 (언리얼 스타일)
            int32 NumSteps = FMath::Min(
                static_cast<int32>(AccumulatedTime / FixedTimestep),
                MaxSubsteps);

            float SimTime = NumSteps * FixedTimestep;
            AccumulatedTime -= SimTime;

            // Substep 수 기록
            StatManager.SetSubstepCount(NumSteps);

            // 시뮬레이션 시작 (비블로킹 - fetchResults 호출하지 않음)
            // 쓰기 잠금으로 시뮬레이션 보호
            {
                SCOPED_SCENE_WRITE_LOCK(PScene);
                bIsSimulating = true;

                // TODO: VehicleUpdates와 PxScene 시뮬레이션이 동일 서브스텝으로 맞춰지지 않음(필요 시 재조정)
                // 서브스텝마다 입력/차량 업데이트 적용
                for (int32 Step = 0; Step < NumSteps; ++Step)
                {
                    ApplyVehicleInputs(FixedTimestep);
                    SimulateVehicles(FixedTimestep);
                }

                // simulate() 타이밍 측정
                uint64 SimStartCycles = FPlatformTime::Cycles64();
                PScene->simulate(SimTime);
                uint64 SimEndCycles = FPlatformTime::Cycles64();
                StatManager.RecordSimulateTime(FPlatformTime::ToMilliseconds(SimEndCycles - SimStartCycles));

                bSimulationPending = true;
            }
        }
        else
        {
            StatManager.SetSubstepCount(0);
        }

        // 보간 Alpha 업데이트 (시뮬레이션 여부와 무관하게 매 프레임 수행)
        StatManager.SetAccumulatedTimeRatio(GetInterpolationAlpha());

        // ═══════════════════════════════════════════════════════════════════════
        // NvCloth 시뮬레이션 (비동기 모드에서도 동작)
        // ═══════════════════════════════════════════════════════════════════════
        if (FClothCore::GetInstance().IsInitialized())
        {
            nv::cloth::Solver* ClothSolver = FClothCore::GetInstance().GetSolver();
            if (ClothSolver)
            {
                int32 NumCloths = ClothSolver->getNumCloths();

                static bool bLoggedClothSim = false;
                if (!bLoggedClothSim && NumCloths > 0)
                {
                    bLoggedClothSim = true;
                    UE_LOG("[PhysScene] Cloth simulation running: %d cloths, DeltaTime=%.4f\n", NumCloths, DeltaSeconds);
                }

                if (NumCloths > 0)
                {
                    ClothSolver->beginSimulation(DeltaSeconds);

                    int32 NumChunks = ClothSolver->getSimulationChunkCount();
                    for (int32 i = 0; i < NumChunks; ++i)
                    {
                        ClothSolver->simulateChunk(i);
                    }

                    ClothSolver->endSimulation();
                }
            }
        }

        // 렌더 보간은 FetchResults() 이후에 수행 (getActiveActors는 시뮬레이션 중 호출 불가)
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 동기 물리 시뮬레이션 (Fallback)
    // ═══════════════════════════════════════════════════════════════════════
    // bAsyncSimulation = false 시 기존 동기 방식으로 동작

    // 비정상적으로 큰 DeltaTime 제한 (예: 디버거 일시정지 후)
    DeltaSeconds = FMath::Min(DeltaSeconds, MaxSubsteps * FixedTimestep);
    AccumulatedTime += DeltaSeconds;

    // 누적 시간이 FixedTimestep에 도달할 때마다 물리 스텝 실행
    int32 NumSteps = 0;
    double TotalSimTime = 0.0;
    double TotalFetchTime = 0.0;

    while (AccumulatedTime >= FixedTimestep && NumSteps < MaxSubsteps)
    {
        // 쓰기 잠금으로 시뮬레이션 보호
        SCOPED_SCENE_WRITE_LOCK(PScene);
        bIsSimulating = true;

        // 입력을 시뮬레이션 직전에 적용 (서브스텝마다)
        ApplyVehicleInputs(FixedTimestep);
        SimulateVehicles(FixedTimestep);

        // simulate() 타이밍 측정
        uint64 SimStartCycles = FPlatformTime::Cycles64();
        PScene->simulate(FixedTimestep);
        uint64 SimEndCycles = FPlatformTime::Cycles64();
        TotalSimTime += FPlatformTime::ToMilliseconds(SimEndCycles - SimStartCycles);

        // fetchResults() 타이밍 측정
        uint64 FetchStartCycles = FPlatformTime::Cycles64();
        PScene->fetchResults(true);
        uint64 FetchEndCycles = FPlatformTime::Cycles64();
        TotalFetchTime += FPlatformTime::ToMilliseconds(FetchEndCycles - FetchStartCycles);

        bIsSimulating = false;

        AccumulatedTime -= FixedTimestep;
        NumSteps++;
    }

    // 동기 모드 통계 기록
    StatManager.SetSubstepCount(NumSteps);
    StatManager.RecordSimulateTime(TotalSimTime);
    StatManager.RecordFetchResultsTime(TotalFetchTime);
    StatManager.SetAccumulatedTimeRatio(GetInterpolationAlpha());

    // 물리 스텝 실행됨 → Transform + Velocity 캡처
    if (NumSteps > 0)
    {
        CaptureActiveActorsTransform();
        CaptureActiveActorsVelocity();
    }

    // 렌더 보간 타이밍 측정
    uint64 InterpStartCycles = FPlatformTime::Cycles64();
    float Alpha = GetInterpolationAlpha();
    UpdateRenderInterpolation(Alpha);
    uint64 InterpEndCycles = FPlatformTime::Cycles64();
    StatManager.RecordInterpolationUpdateTime(FPlatformTime::ToMilliseconds(InterpEndCycles - InterpStartCycles));

    // ═══════════════════════════════════════════════════════════════════════
    // NvCloth 시뮬레이션 (동기 모드)
    // ═══════════════════════════════════════════════════════════════════════
    if (NumSteps > 0 && FClothCore::GetInstance().IsInitialized())
    {
        nv::cloth::Solver* ClothSolver = FClothCore::GetInstance().GetSolver();
        if (ClothSolver)
        {
            int32 NumCloths = ClothSolver->getNumCloths();
            if (NumCloths > 0)
            {
                float ClothDeltaTime = NumSteps * FixedTimestep;
                ClothSolver->beginSimulation(ClothDeltaTime);

                int32 NumChunks = ClothSolver->getSimulationChunkCount();
                for (int32 i = 0; i < NumChunks; ++i)
                {
                    ClothSolver->simulateChunk(i);
                }

                ClothSolver->endSimulation();
            }
        }
    }
}

void FPhysSceneImpl::FetchResults()
{
    if (!PScene)
        return;

    FPhysicsStatManager& StatManager = FPhysicsStatManager::GetInstance();

    // ═══════════════════════════════════════════════════════════════════════
    // 비동기 모드: 렌더링 전 결과 수집
    // ═══════════════════════════════════════════════════════════════════════
    // EndFrame()에서 호출되며, 시뮬레이션이 진행 중인 경우
    // 렌더링 전에 결과를 수집하여 동기화합니다.

    if (bAsyncSimulation && bSimulationPending)
    {
        // 쓰기 잠금으로 결과 수집 보호
        {
            SCOPED_SCENE_WRITE_LOCK(PScene);

            // fetchResults() 타이밍 측정
            uint64 FetchStartCycles = FPlatformTime::Cycles64();
            PScene->fetchResults(true);
            uint64 FetchEndCycles = FPlatformTime::Cycles64();
            StatManager.RecordFetchResultsTime(FPlatformTime::ToMilliseconds(FetchEndCycles - FetchStartCycles));

            bSimulationPending = false;
            bIsSimulating = false;
        }

        // Transform + Velocity 캡처
        CaptureActiveActorsTransform();
        CaptureActiveActorsVelocity();

        // 렌더 보간 업데이트 (캡처된 Actor 기준으로 수행)
        uint64 InterpStartCycles = FPlatformTime::Cycles64();
        float Alpha = GetInterpolationAlpha();
        UpdateRenderInterpolation(Alpha);
        uint64 InterpEndCycles = FPlatformTime::Cycles64();
        StatManager.RecordInterpolationUpdateTime(FPlatformTime::ToMilliseconds(InterpEndCycles - InterpStartCycles));

        // 지연된 명령 처리 (다음 시뮬레이션에 반영)
        ProcessPendingCommands();
    }
    else if (bAsyncSimulation)
    {
        // bSimulationPending = false인 경우에도 렌더 보간 업데이트 (Fixed Timestep 미도달)
        uint64 InterpStartCycles = FPlatformTime::Cycles64();
        float Alpha = GetInterpolationAlpha();
        UpdateRenderInterpolation(Alpha);
        uint64 InterpEndCycles = FPlatformTime::Cycles64();
        StatManager.RecordInterpolationUpdateTime(FPlatformTime::ToMilliseconds(InterpEndCycles - InterpStartCycles));
    }

    // Actor 통계 업데이트
    PxU32 NumActiveActors = 0;
    PScene->getActiveActors(NumActiveActors);
    StatManager.SetActiveActorCount(NumActiveActors);
    StatManager.SetDynamicActorCount(GetNumActors(PxActorTypeFlag::eRIGID_DYNAMIC));
    StatManager.SetStaticActorCount(GetNumActors(PxActorTypeFlag::eRIGID_STATIC));
    StatManager.SetPendingCommandCount(PendingCommands.Num());

    // 동기 모드에서는 Simulate()에서 이미 처리 완료
}

void FPhysSceneImpl::SyncActiveActorsToComponents()
{
    if (!PScene)
        return;

    // Active Actors Transform 동기화 (레거시 - 보간 미사용 시)
    PxU32 NumActiveActors = 0;
    PxActor** ActiveActors = PScene->getActiveActors(NumActiveActors);

    for (PxU32 i = 0; i < NumActiveActors; ++i)
    {
        if (PxRigidActor* RigidActor = ActiveActors[i]->is<PxRigidActor>())
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(RigidActor->userData);
            if (BodyInst && BodyInst->IsInScene())
            {
                BodyInst->SyncPhysicsToComponent();
            }
        }
    }
}

void FPhysSceneImpl::CaptureActiveActorsTransform()
{
    if (!PScene)
        return;

    // Active Actors의 Transform 이력 캡처
    PxU32 NumActiveActors = 0;
    PxActor** ActiveActors = PScene->getActiveActors(NumActiveActors);

    for (PxU32 i = 0; i < NumActiveActors; ++i)
    {
        if (PxRigidActor* RigidActor = ActiveActors[i]->is<PxRigidActor>())
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(RigidActor->userData);
            if (BodyInst && BodyInst->IsInScene() && BodyInst->bSimulatePhysics)
            {
                BodyInst->CapturePhysicsTransform();
            }
        }
    }
}

void FPhysSceneImpl::UpdateRenderInterpolation(float Alpha)
{
    if (!PScene)
        return;

    // Active Actors의 렌더 보간 Transform 업데이트
    PxU32 NumActiveActors = 0;
    PxActor** ActiveActors = PScene->getActiveActors(NumActiveActors);

    for (PxU32 i = 0; i < NumActiveActors; ++i)
    {
        if (PxRigidActor* RigidActor = ActiveActors[i]->is<PxRigidActor>())
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(RigidActor->userData);
            if (BodyInst && BodyInst->IsInScene() && BodyInst->bSimulatePhysics)
            {
                BodyInst->UpdateRenderInterpolation(Alpha);
            }
        }
    }
}

void FPhysSceneImpl::SetGravity(const PxVec3& InGravity)
{
    if (PScene)
    {
        PScene->setGravity(InGravity);
    }
}

PxVec3 FPhysSceneImpl::GetGravity() const
{
    if (PScene)
    {
        return PScene->getGravity();
    }
    return PxVec3(0.0f, -9.81f, 0.0f);
}

uint32 FPhysSceneImpl::GetNumActors(PxActorTypeFlags Types) const
{
    if (PScene)
    {
        return PScene->getNbActors(Types);
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Actor 추가/제거 (스레드 안전, 지연 처리 지원)
// ═══════════════════════════════════════════════════════════════════════════════

void FPhysSceneImpl::AddActor(PxActor* Actor)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::AddActor;
        // PxActor를 PxRigidActor로 캐스팅 (Scene에 추가되는 Actor는 항상 RigidActor)
        Cmd.Actor = static_cast<PxRigidActor*>(Actor);
        PendingCommands.Add(Cmd);
        return;
    }

    // 쓰기 잠금으로 Actor 추가 보호
    SCOPED_SCENE_WRITE_LOCK(PScene);
    PScene->addActor(*Actor);
}

void FPhysSceneImpl::RemoveActor(PxActor* Actor)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::RemoveActor;
        // PxActor를 PxRigidActor로 캐스팅 (Scene에 추가되는 Actor는 항상 RigidActor)
        Cmd.Actor = static_cast<PxRigidActor*>(Actor);
        PendingCommands.Add(Cmd);
        return;
    }

    // 쓰기 잠금으로 Actor 제거 보호
    SCOPED_SCENE_WRITE_LOCK(PScene);
    PScene->removeActor(*Actor);
}

void FPhysSceneImpl::ProcessPendingCommands()
{
    if (PendingCommands.Num() == 0)
        return;

    // 쓰기 잠금으로 지연 명령 처리 보호
    SCOPED_SCENE_WRITE_LOCK(PScene);

    for (const FPhysicsCommand& Cmd : PendingCommands)
    {
        switch (Cmd.Type)
        {
        case FPhysicsCommand::EType::AddActor:
            if (Cmd.Actor)
            {
                PScene->addActor(*Cmd.Actor);
            }
            break;

        case FPhysicsCommand::EType::RemoveActor:
            if (Cmd.Actor)
            {
                PScene->removeActor(*Cmd.Actor);
            }
            break;

        case FPhysicsCommand::EType::SetGlobalPose:
            if (Cmd.Actor)
            {
                Cmd.Actor->setGlobalPose(Cmd.Transform);
            }
            break;

        case FPhysicsCommand::EType::AddForce:
            if (PxRigidDynamic* Dynamic = Cmd.Actor ? Cmd.Actor->is<PxRigidDynamic>() : nullptr)
            {
                Dynamic->addForce(Cmd.Vector, Cmd.ForceMode);
            }
            break;

        case FPhysicsCommand::EType::AddTorque:
            if (PxRigidDynamic* Dynamic = Cmd.Actor ? Cmd.Actor->is<PxRigidDynamic>() : nullptr)
            {
                Dynamic->addTorque(Cmd.Vector, Cmd.ForceMode);
            }
            break;

        case FPhysicsCommand::EType::SetLinearVelocity:
            if (PxRigidDynamic* Dynamic = Cmd.Actor ? Cmd.Actor->is<PxRigidDynamic>() : nullptr)
            {
                PxVec3 NewVel = Cmd.Vector;
                if (Cmd.bAddToCurrent)
                {
                    NewVel += Dynamic->getLinearVelocity();
                }
                Dynamic->setLinearVelocity(NewVel);
            }
            break;

        case FPhysicsCommand::EType::SetAngularVelocity:
            if (PxRigidDynamic* Dynamic = Cmd.Actor ? Cmd.Actor->is<PxRigidDynamic>() : nullptr)
            {
                PxVec3 NewAngVel = Cmd.Vector;
                if (Cmd.bAddToCurrent)
                {
                    NewAngVel += Dynamic->getAngularVelocity();
                }
                Dynamic->setAngularVelocity(NewAngVel);
            }
            break;
        }
    }

    PendingCommands.Empty();
}

int32 FPhysSceneImpl::CalculateOptimalThreadCount()
{
    // C++ 표준 라이브러리로 논리 프로세서 수 조회
    uint32 NumCores = std::thread::hardware_concurrency();

    if (NumCores == 0)
    {
        // 조회 실패 시 기본값
        return 4;
    }

    // 언리얼 엔진 스타일: Max(코어수 - 1, 1)
    // - 메인 스레드용 1개 확보 (-1)
    // - 최소 1개 보장
    int32 OptimalCount = FMath::Max(static_cast<int32>(NumCores) - 1, 1);

    return OptimalCount;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 물리 상태 변경 (스레드 안전, 시뮬레이션 중 지연 처리)
// ═══════════════════════════════════════════════════════════════════════════════

void FPhysSceneImpl::SetActorGlobalPose(PxRigidActor* Actor, const PxTransform& Pose)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::SetGlobalPose;
        Cmd.Actor = Actor;
        Cmd.Transform = Pose;
        PendingCommands.Add(Cmd);
        return;
    }

    // 즉시 실행
    SCOPED_SCENE_WRITE_LOCK(PScene);
    Actor->setGlobalPose(Pose);
}

void FPhysSceneImpl::AddForceToActor(PxRigidDynamic* Actor, const PxVec3& Force, PxForceMode::Enum Mode)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::AddForce;
        Cmd.Actor = Actor;
        Cmd.Vector = Force;
        Cmd.ForceMode = Mode;
        PendingCommands.Add(Cmd);
        return;
    }

    // 즉시 실행
    SCOPED_SCENE_WRITE_LOCK(PScene);
    Actor->addForce(Force, Mode);
}

void FPhysSceneImpl::AddTorqueToActor(PxRigidDynamic* Actor, const PxVec3& Torque, PxForceMode::Enum Mode)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::AddTorque;
        Cmd.Actor = Actor;
        Cmd.Vector = Torque;
        Cmd.ForceMode = Mode;
        PendingCommands.Add(Cmd);
        return;
    }

    // 즉시 실행
    SCOPED_SCENE_WRITE_LOCK(PScene);
    Actor->addTorque(Torque, Mode);
}

void FPhysSceneImpl::SetActorLinearVelocity(PxRigidDynamic* Actor, const PxVec3& Velocity, bool bAddToCurrent)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::SetLinearVelocity;
        Cmd.Actor = Actor;
        Cmd.Vector = Velocity;
        Cmd.bAddToCurrent = bAddToCurrent;
        PendingCommands.Add(Cmd);
        return;
    }

    // 즉시 실행
    SCOPED_SCENE_WRITE_LOCK(PScene);
    PxVec3 NewVel = Velocity;
    if (bAddToCurrent)
    {
        NewVel += Actor->getLinearVelocity();
    }
    Actor->setLinearVelocity(NewVel);
}

void FPhysSceneImpl::SetActorAngularVelocity(PxRigidDynamic* Actor, const PxVec3& AngVel, bool bAddToCurrent)
{
    if (!PScene || !Actor)
        return;

    // 시뮬레이션 중이면 지연 처리
    if (bIsSimulating)
    {
        FPhysicsCommand Cmd;
        Cmd.Type = FPhysicsCommand::EType::SetAngularVelocity;
        Cmd.Actor = Actor;
        Cmd.Vector = AngVel;
        Cmd.bAddToCurrent = bAddToCurrent;
        PendingCommands.Add(Cmd);
        return;
    }

    // 즉시 실행
    SCOPED_SCENE_WRITE_LOCK(PScene);
    PxVec3 NewAngVel = AngVel;
    if (bAddToCurrent)
    {
        NewAngVel += Actor->getAngularVelocity();
    }
    Actor->setAngularVelocity(NewAngVel);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Velocity 캐싱 (읽기 작업 스레드 안전)
// ═══════════════════════════════════════════════════════════════════════════════

void FPhysSceneImpl::CaptureActiveActorsVelocity()
{
    if (!PScene)
        return;

    // Active Actors의 Velocity 캡처
    PxU32 NumActiveActors = 0;
    PxActor** ActiveActors = PScene->getActiveActors(NumActiveActors);

    for (PxU32 i = 0; i < NumActiveActors; ++i)
    {
        if (PxRigidDynamic* Dynamic = ActiveActors[i]->is<PxRigidDynamic>())
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(Dynamic->userData);
            if (BodyInst && BodyInst->IsInScene() && BodyInst->bSimulatePhysics)
            {
                BodyInst->CapturePhysicsVelocity();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Vehicle 컴포넌트 레지스트리
// ═══════════════════════════════════════════════════════════════════════════════

void FPhysSceneImpl::RegisterVehicleComponent(USimpleWheeledVehicleMovementComponent* InComponent)
{
    if (!InComponent)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(VehicleComponentMutex);
    VehicleComponents.AddUnique(TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>(InComponent));
    bVehicleListDirty = true;
}

void FPhysSceneImpl::UnregisterVehicleComponent(USimpleWheeledVehicleMovementComponent* InComponent)
{
    if (!InComponent)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(VehicleComponentMutex);
    bool bRemoved = VehicleComponents.Remove(TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>(InComponent));
    if (bRemoved)
    {
        bVehicleListDirty = true;
    }
}

const TArray<TWeakObjectPtr<USimpleWheeledVehicleMovementComponent>>& FPhysSceneImpl::GetVehicleComponents()
{
    CompactVehicleComponents();
    return VehicleComponents;
}

void FPhysSceneImpl::CompactVehicleComponents()
{
    std::lock_guard<std::mutex> Lock(VehicleComponentMutex);

    if (!bVehicleListDirty)
    {
        return;
    }

    for (int32 Index = VehicleComponents.Num() - 1; Index >= 0; --Index)
    {
        if (!VehicleComponents[Index].IsValid())
        {
            VehicleComponents.RemoveAt(Index);
        }
    }

    VehicleComponents.Shrink();
    bVehicleListDirty = false;
}
