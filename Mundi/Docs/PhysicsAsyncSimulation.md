# 비동기 물리 시뮬레이션 — 구현

PhysX 시뮬레이션을 게임 스레드와 겹쳐 돌리는 구조입니다. 이 문서는 **무엇이 어떤 순서로
실행되고, 왜 그 순서여야 하는지**를 설명합니다.

- 측정 결과는 [PhysicsAsyncProfiling.md](PhysicsAsyncProfiling.md) 에 있습니다 — 8000 dynamic body 에서 프레임 시간 17.6% 감소
- 잠금 설계의 원안은 [PhysicsMultithread.md](PhysicsMultithread.md) 에 있습니다

---

## 1. 출발점 — PhysX 의 `simulate` / `fetchResults`

PhysX 는 한 스텝을 두 호출로 나눠 놓았습니다.

| 호출 | 동작 |
|---|---|
| `PxScene::simulate(dt)` | 시뮬레이션을 **시작**하고 바로 반환합니다. 실제 계산은 worker 스레드에서 진행됩니다 |
| `PxScene::fetchResults(true)` | 계산이 끝날 때까지 **블로킹 대기**하고, 결과를 씬에 반영하며 충돌 이벤트 콜백을 부릅니다 |

두 호출을 붙여 두면 `simulate` 가 시작한 일을 `fetchResults` 가 곧바로 기다리므로,
게임 스레드는 물리가 끝날 때까지 아무것도 하지 못합니다. PhysX 가 내부적으로
멀티스레드여도 **게임 스레드는 놀고 있습니다.**

두 호출 사이에 게임 스레드의 다른 일을 끼워 넣는 것이 이 구현의 전부입니다.

---

## 2. 한 프레임의 실행 순서

`UWorld::Tick` 이 프레임을 이 순서로 진행합니다
([World.cpp:280-350](../Source/Runtime/Engine/GameFramework/World.cpp#L280-L350)).

```
UWorld::Tick(DeltaSeconds)
│
├─ 1  PhysScene->StartFrame()      ─ 통계 리셋, 차량 raycast
│
├─ 2  PhysScene->Tick(dt)          ─ simulate() 호출하고 즉시 반환  ┐
│                                                                  │ 이 구간이
├─ 3  Actor Tick 순회                                              │ PhysX worker
│     LuaManager->Tick()                                           │ 스레드와
│     ProcessPendingKillActors()                                   │ 겹쳐서 돈다
│     CollisionManager->UpdateCollisions()                         │
│                                                                  ┘
└─ 4  PhysScene->EndFrame()        ─ fetchResults(true) 로 완료 대기
```

핵심은 **2번과 4번 사이에 3번이 들어간다**는 것입니다. 2번에서 시작한 시뮬레이션이
worker 스레드에서 도는 동안 게임 스레드는 3번을 처리하고, 4번에서 비로소 기다립니다.

겹치는 구간의 비용이 물리 계산 시간보다 길면 **대기 시간이 0 에 수렴**합니다.
따라서 줄일 수 있는 시간은 `min(물리 계산 시간, 3번 구간 비용)` 을 넘지 못합니다.
이 상한은 측정에서도 그대로 확인됩니다.

### 클래스 구조

```
FPhysScene          외부에 노출되는 얼굴. PhysX 헤더를 include 하지 않는다
└─ FPhysSceneImpl   PxScene 을 직접 다루는 구현 (PIMPL)
```

`FPhysScene::StartFrame` / `Tick` / `EndFrame`
([PhysScene.cpp:99](../Source/Runtime/Engine/Physics/PhysScene.cpp#L99),
[141](../Source/Runtime/Engine/Physics/PhysScene.cpp#L141),
[181](../Source/Runtime/Engine/Physics/PhysScene.cpp#L181))
이 바깥 인터페이스이고, 실제 작업은
`FPhysSceneImpl::Simulate` ([PhysScene.cpp:440](../Source/Runtime/Engine/Physics/PhysScene.cpp#L440))
과 `FPhysSceneImpl::FetchResults` ([PhysScene.cpp:661](../Source/Runtime/Engine/Physics/PhysScene.cpp#L661))
가 합니다.

PIMPL 로 나눈 이유는 PhysX 타입이 엔진 헤더로 새어 나가지 않게 하기 위해서입니다.
`FBodyInstance` 도 같은 방식입니다 — 헤더에는 PhysX 전방 선언만 있습니다
([BodyInstance.h](../Source/Runtime/Engine/Physics/BodyInstance.h)).

---

## 3. Fixed timestep 과 substep

물리는 프레임 시간이 아니라 **고정 간격 1/60 초**로 전진합니다
([PhysSceneImpl.h:129](../Source/Runtime/Engine/Physics/PhysSceneImpl.h#L129)).
프레임레이트가 흔들려도 시뮬레이션 결과가 달라지지 않게 하기 위해서입니다.

`Simulate()` 가 프레임 델타를 `AccumulatedTime` 에 더하고, 1/60 이 모일 때마다 스텝을
꺼내 씁니다 ([PhysScene.cpp:486-528](../Source/Runtime/Engine/Physics/PhysScene.cpp#L486-L528)).

```cpp
AccumulatedTime += DeltaSeconds;

if (AccumulatedTime >= FixedTimestep)
{
    int32 NumSteps = FMath::Min(int32(AccumulatedTime / FixedTimestep), MaxSubsteps);
    float SimTime = NumSteps * FixedTimestep;
    AccumulatedTime -= SimTime;

    PScene->simulate(SimTime);   // 비블로킹
    bSimulationPending = true;
}
```

- **프레임이 짧으면** 1/60 이 안 모여 그 프레임은 물리 스텝을 건너뜁니다.
- **프레임이 길면** 밀린 시간만큼 여러 스텝을 한 번에 처리합니다(substep).
- `MaxSubsteps = 8` ([PhysSceneImpl.h:132](../Source/Runtime/Engine/Physics/PhysSceneImpl.h#L132))
  이 상한입니다. 이 값에 닿으면 물리가 실시간을 따라가지 못하고 느려집니다 —
  느린 프레임이 더 많은 물리 작업을 부르고 그게 다시 프레임을 늘리는 악순환을 끊기 위한 상한입니다.

### 렌더 보간

물리는 1/60 로 뛰는데 화면은 그보다 자주 그려집니다. 그대로 두면 물체가 끊겨 보입니다.

그래서 바디마다 **직전 스텝과 현재 스텝의 transform 두 개**를 들고 있다가, 그 사이를
보간해 컴포넌트에 씁니다.

- `FBodyInstance::CapturePhysicsTransform()` — 현재를 직전으로 밀고 새 값을 읽습니다
  ([BodyInstance.cpp:385](../Source/Runtime/Engine/Physics/BodyInstance.cpp#L385))
- `FBodyInstance::UpdateRenderInterpolation(Alpha)` — 두 transform 을 `Lerp`(회전은 `Slerp`)
  해서 컴포넌트에 적용합니다 ([BodyInstance.cpp:402](../Source/Runtime/Engine/Physics/BodyInstance.cpp#L402))
- `Alpha = AccumulatedTime / FixedTimestep` — 다음 스텝까지 얼마나 왔는지의 비율입니다
  ([PhysSceneImpl.h:66](../Source/Runtime/Engine/Physics/PhysSceneImpl.h#L66))

보간은 반드시 `fetchResults()` **이후에** 해야 합니다. 보간 대상 목록을
`PxScene::getActiveActors()` 로 얻는데, 이 함수는 시뮬레이션이 진행 중일 때 호출할 수 없기
때문입니다. 그래서 `FetchResults()` 안에서 캡처 → 보간 순으로 처리합니다
([PhysScene.cpp:690-700](../Source/Runtime/Engine/Physics/PhysScene.cpp#L690-L700)).

---

## 4. 겹치는 동안의 안전성

2번과 4번 사이에 게임 코드가 도는 동안, PhysX 씬은 worker 스레드가 쓰고 있습니다.
이 구간에서 게임 코드가 씬을 건드리면 문제가 됩니다. 읽기와 쓰기를 다르게 처리합니다.

### 읽기 — 직전 스텝의 값을 본다

액터 Tick 은 보통 바디의 위치를 읽습니다. `FBodyInstance::GetWorldTransform()` 은
scene read lock 을 잡고 `getGlobalPose()` 를 읽습니다
([BodyInstance.cpp:298](../Source/Runtime/Engine/Physics/BodyInstance.cpp#L298)).

`simulate()` 를 감싼 write lock 은 **`simulate()` 가 반환하자마자 스코프가 끝나며 풀립니다**
([PhysScene.cpp:507-527](../Source/Runtime/Engine/Physics/PhysScene.cpp#L507-L527)).
`simulate()` 는 비블로킹이므로 그 시점에 계산은 아직 진행 중입니다. 따라서 read lock 이
막히지 않고, 겹침이 성립합니다.

그 대가로 **이 프레임의 액터 Tick 이 읽는 pose 는 직전 스텝의 값**입니다. 화면에 나가는
transform 은 4번의 보간이 다시 덮어쓰므로 렌더 결과에는 영향이 없습니다.

씬은 `eREQUIRE_RW_LOCK` 없이 만들어집니다
([PhysScene.cpp:404-406](../Source/Runtime/Engine/Physics/PhysScene.cpp#L404-L406)).
잠금은 `PhysicsSceneLock.h` 의 RAII 래퍼로 겁니다
([PhysicsSceneLock.h](../Source/Runtime/Engine/Physics/PhysicsSceneLock.h)).

### 속도는 캐시해 둔다

위치와 달리 속도는 **캡처해 둔 값을 돌려줍니다**
([BodyInstance.cpp:671](../Source/Runtime/Engine/Physics/BodyInstance.cpp#L671)).

```cpp
FVector FBodyInstance::GetLinearVelocity() const
{
    // 캐시된 값 반환 (PhysX 직접 접근 안 함 - 스레드 안전)
    return PhysicsConversion::ToFVector(Impl->CachedLinearVelocity);
}
```

캡처는 `fetchResults()` 직후에 `CaptureActiveActorsVelocity()` 가 한 번에 합니다.

### 쓰기 — 미뤘다가 나중에 적용한다

시뮬레이션 중에 씬을 수정하는 것은 허용되지 않습니다. 그래서 쓰기 요청은 **큐에 쌓았다가
`fetchResults()` 뒤에 적용**합니다.

```cpp
void FPhysSceneImpl::AddActor(PxActor* Actor)
{
    if (bIsSimulating)          // 시뮬레이션 중이면 지연 처리
    {
        PendingCommands.Add({ EType::AddActor, ... });
        return;
    }

    SCOPED_SCENE_WRITE_LOCK(PScene);
    PScene->addActor(*Actor);   // 아니면 즉시
}
```

([PhysScene.cpp:821](../Source/Runtime/Engine/Physics/PhysScene.cpp#L821))

큐에 담기는 명령은 액터 추가·제거, transform 설정, 힘·토크, 속도 설정입니다
([PhysSceneImpl.h:142-160](../Source/Runtime/Engine/Physics/PhysSceneImpl.h#L142-L160)).
`ProcessPendingCommands()` 가 `FetchResults()` 끝에서 한 번에 비웁니다
([PhysScene.cpp:863](../Source/Runtime/Engine/Physics/PhysScene.cpp#L863)).

게임 코드 입장에서는 평소처럼 `AddImpulse()` 를 부르면 되고, 그 효과가 이번 스텝이 아니라
다음 스텝에 반영될 뿐입니다.

### 충돌 이벤트

`fetchResults()` 가 내부에서 `PxSimulationEventCallback` 을 부릅니다. 즉 이벤트는
게임 스레드가 4번에 들어와 있을 때 전달되며, 별도 동기화가 필요 없습니다
([PhysicsEventCallback.cpp](../Source/Runtime/Engine/Physics/PhysicsEventCallback.cpp)).

---

## 5. 동기 경로가 남아 있는 이유

`bAsyncSimulation` 플래그가 `false` 면 `simulate()` 와 `fetchResults()` 를 붙여서 부르는
예전 경로로 돕니다 ([PhysScene.cpp:574](../Source/Runtime/Engine/Physics/PhysScene.cpp#L574)).

이 경로는 fallback 이자 **측정 기준선**입니다. 비동기 도입 이전 커밋을 checkout 하지 않고도
같은 실행 순서를 재현할 수 있어서, 프로파일링의 A/B 를 이 플래그 하나로 했습니다.
자세한 것은 [PhysicsAsyncProfiling.md](PhysicsAsyncProfiling.md) 를 보세요.

---

## 6. worker 스레드 수

`PxDefaultCpuDispatcherCreate(N)` 의 `N` 을 `Max(논리 코어 수 - 1, 1)` 로 정합니다
([PhysScene.cpp:939](../Source/Runtime/Engine/Physics/PhysScene.cpp#L939)).
UE 의 방식을 따른 것입니다.

> 측정해보니 이 휴리스틱은 테스트 머신(i7-14700HX, P-core 8 + E-core 12)에서 최적이
> 아닙니다. 8000 body 기준 4 스레드가 가장 빠르고, 이 식이 고르는 27 스레드는 1 스레드와
> 차이가 없습니다. 원인은 규명하지 않았습니다.
> [PhysicsAsyncProfiling.md 의 「한계」](PhysicsAsyncProfiling.md#한계) 를 보세요.

---

## 7. 관련 커밋

시간 순입니다.

| 커밋 | 내용 |
|---|---|
| `f358fac4` | Fixed timestep 도입. 프레임 델타 대신 1/60 고정 간격으로 전진 |
| `3d2814f3` | 스텝 사이 렌더 보간. 직전·현재 transform 을 들고 `Lerp` |
| `5292ec1c` | **비동기 전환.** `simulate()` 와 `fetchResults()` 분리 |
| `0e1ad224` | **`EndFrame()` 을 Actor Tick 뒤로 이동.** 겹침이 실제로 생기는 지점 |
| `a22562e2` | `PhysicsSceneLock` — scene read/write lock RAII 래퍼 |
| `a5d81a75` | worker 스레드 수를 `Max(논리 코어 - 1, 1)` 로 |
| `92ee6c58` | `stat physics` — 시뮬레이션·fetch 시간, 액터·이벤트 수 오버레이 |
| `03e44793` | 시뮬레이션 중 쓰기 지연(`PendingCommands`)과 속도 읽기 캐싱 |
| `e150ba2a` | 렌더 보간을 `FetchResults()` 이후로 이동 |
| `48d48e3d` | `FPhysScene` 이동 대입 시 상수 멤버를 건드리던 버그 수정 |

`5292ec1c` 단독으로는 효과가 없습니다. 그 시점에는 `StartFrame()` 에서 결과를 수집했고
`UWorld::Tick` 이 `StartFrame`·`Tick`·`EndFrame` 을 연달아 불러 겹칠 구간이 없었습니다.
12분 뒤 `0e1ad224` 가 `EndFrame()` 을 Actor Tick 뒤로 옮기면서 비로소 겹침이 생깁니다.
두 커밋이 한 쌍입니다.

---

## 8. 직접 보기

| 무엇 | 어디 |
|---|---|
| 프레임 순서 | [`World.cpp:280-350`](../Source/Runtime/Engine/GameFramework/World.cpp#L280-L350) |
| 외부 인터페이스 | [`PhysScene.h`](../Source/Runtime/Engine/Physics/PhysScene.h) |
| 비동기 `simulate` | [`PhysScene.cpp:486-528`](../Source/Runtime/Engine/Physics/PhysScene.cpp#L486-L528) |
| 동기 fallback | [`PhysScene.cpp:574-620`](../Source/Runtime/Engine/Physics/PhysScene.cpp#L574-L620) |
| `fetchResults` 와 후처리 | [`PhysScene.cpp:661-715`](../Source/Runtime/Engine/Physics/PhysScene.cpp#L661-L715) |
| 쓰기 지연 큐 | [`PhysScene.cpp:821-900`](../Source/Runtime/Engine/Physics/PhysScene.cpp#L821-L900) |
| 잠금 래퍼 | [`PhysicsSceneLock.h`](../Source/Runtime/Engine/Physics/PhysicsSceneLock.h) |
| 바디 보간·캐시 | [`BodyInstance.cpp:298-430`](../Source/Runtime/Engine/Physics/BodyInstance.cpp#L298-L430) |
| 통계 구조체 | [`PhysicsStats.h`](../Source/Runtime/Engine/Physics/PhysicsStats.h) |

실행 중에는 콘솔에 `stat physics` 를 입력하면 시뮬레이션·fetch 시간, 활성 바디 수,
substep 수, worker 스레드 수가 화면에 뜹니다.
