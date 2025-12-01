# 물리 시뮬레이션 멀티스레드 지원 계획

## 개요

PhysX 기반 물리 시뮬레이션에 멀티스레드 안전성을 추가하는 작업입니다.
언리얼 엔진의 Chaos Physics 시스템을 참고하여 구현합니다.

---

## 언리얼 엔진 분석 결과

### Chaos Physics의 멀티스레드 구현 방식

언리얼 엔진은 `Threading.h` 파일에서 다양한 Scene Lock 타입을 제공합니다:

**1. Lock 타입 옵션** (`CHAOS_SCENE_LOCK_TYPE` 매크로):

- `FPhysicsSceneGuard` - 재귀적 RW Lock (TLS 기반)
- `TRwFifoLock<FPhysSpinLock>` - 공정한 RW SpinLock (비양보)
- `TRwFifoLock<FCriticalSection>` - 공정한 RW Lock (양보)
- `FPhysicsRwLock` - 플랫폼별 FRWLock 기반 재귀적 Lock

**2. RAII Scoped Lock 클래스**:

- `TPhysicsSceneGuardScopedWrite<MutexType>` - 쓰기 잠금
- `TPhysicsSceneGuardScopedRead<MutexType>` - 읽기 잠금
- `FScopedSceneLock_Chaos` - Actor/Constraint 기반 자동 Scene Lock

**3. `_AssumesLocked` 함수 네이밍 컨벤션**:

- Lock이 이미 획득된 상태에서 호출해야 하는 함수에 `_AssumesLocked` 접미사 사용
- 예: `AddForce_AssumesLocked()`, `SetKinematicTarget_AssumesLocked()`

**4. Thread Context 추적** (`FPhysicsThreadContext`):

- `IsInPhysicsSimContext()` - 물리 시뮬레이션 스레드인지 확인
- `IsInGameThreadContext()` - 게임 스레드인지 확인
- Lock 획득 시 자동으로 Context 전환

---

## 현재 Mundi 엔진 상태

### 이미 구현된 부분

- `PxDefaultCpuDispatcher` 4개 스레드로 생성됨 (`PhysScene.cpp:229`)
- 비동기 시뮬레이션 지원 (`bAsyncSimulation` 플래그)
- `PxSceneFlag::eENABLE_ACTIVE_ACTORS`, `eENABLE_CCD`, `eENABLE_PCM` 플래그 설정됨

### 미구현 부분 (멀티스레드 안전성 취약)

- Scene Lock 시스템 없음
- `SCOPED_READ_LOCK` / `SCOPED_WRITE_LOCK` 매크로 없음
- Simulate 중 Actor 추가/제거 시 데이터 레이스 위험
- EventCallback에서 메인 스레드와 동시 접근 위험

---

## 구현 계획 (간단한 버전)

PhysX 내장 `PxScene::lockRead()/lockWrite()`를 사용하는 RAII 래퍼로 구현합니다.

### 1단계: Scene Lock 시스템 구현

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysicsSceneLock.h` (신규)

```cpp
#pragma once
#include <PxPhysicsAPI.h>

// RAII 읽기 잠금
class FPhysSceneReadLock
{
public:
    explicit FPhysSceneReadLock(physx::PxScene* InScene)
        : Scene(InScene)
    {
        if (Scene) Scene->lockRead();
    }

    ~FPhysSceneReadLock()
    {
        if (Scene) Scene->unlockRead();
    }

    // 복사/이동 금지
    FPhysSceneReadLock(const FPhysSceneReadLock&) = delete;
    FPhysSceneReadLock& operator=(const FPhysSceneReadLock&) = delete;

private:
    physx::PxScene* Scene;
};

// RAII 쓰기 잠금
class FPhysSceneWriteLock
{
public:
    explicit FPhysSceneWriteLock(physx::PxScene* InScene)
        : Scene(InScene)
    {
        if (Scene) Scene->lockWrite();
    }

    ~FPhysSceneWriteLock()
    {
        if (Scene) Scene->unlockWrite();
    }

    FPhysSceneWriteLock(const FPhysSceneWriteLock&) = delete;
    FPhysSceneWriteLock& operator=(const FPhysSceneWriteLock&) = delete;

private:
    physx::PxScene* Scene;
};

// 편의 매크로
#define SCOPED_READ_LOCK(Scene) FPhysSceneReadLock ScopedReadLock_##__LINE__(Scene)
#define SCOPED_WRITE_LOCK(Scene) FPhysSceneWriteLock ScopedWriteLock_##__LINE__(Scene)
```

---

### 2단계: PhysSceneImpl에 Lock 적용

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysSceneImpl.h`

- `#include "PhysicsSceneLock.h"` 추가

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysScene.cpp`

#### 2.1 Simulate 함수에 쓰기 잠금 추가

```cpp
void FPhysSceneImpl::Simulate(float DeltaSeconds)
{
    if (!PScene) return;

    // 쓰기 잠금으로 시뮬레이션 보호
    SCOPED_WRITE_LOCK(PScene);

    // ... 기존 로직
}
```

#### 2.2 FetchResults 함수에 쓰기 잠금 추가

```cpp
void FPhysSceneImpl::FetchResults()
{
    if (!PScene) return;

    // fetchResults도 쓰기 잠금 필요 (씬 상태 변경)
    SCOPED_WRITE_LOCK(PScene);

    // ... 기존 로직
}
```

#### 2.3 Actor 추가/제거 함수에 쓰기 잠금 추가

```cpp
void FPhysSceneImpl::AddActor(physx::PxActor* Actor)
{
    if (!PScene || !Actor) return;

    SCOPED_WRITE_LOCK(PScene);
    PScene->addActor(*Actor);
}

void FPhysSceneImpl::RemoveActor(physx::PxActor* Actor)
{
    if (!PScene || !Actor) return;

    SCOPED_WRITE_LOCK(PScene);
    PScene->removeActor(*Actor);
}
```

---

### 3단계: BodyInstance에 Lock 적용

**파일**: `Mundi/Source/Runtime/Engine/Physics/BodyInstance.cpp`

Transform 조회 함수에 읽기 잠금 추가:

```cpp
FTransform FBodyInstance::GetWorldTransform() const
{
    if (!Impl || !Impl->RigidActor) return FTransform::Identity;

    // 읽기 잠금으로 Transform 접근 보호
    SCOPED_READ_LOCK(OwningScene->GetPxScene());

    // ... 기존 로직
}
```

Transform 설정 함수에 쓰기 잠금 추가:

```cpp
void FBodyInstance::SetWorldTransform(const FTransform& NewTransform, bool bTeleport)
{
    if (!Impl || !Impl->RigidActor) return;

    SCOPED_WRITE_LOCK(OwningScene->GetPxScene());

    // ... 기존 로직
}
```

---

### 4단계: EventCallback 안전성 강화

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysicsEventCallback.cpp`

```cpp
void FPhysicsEventCallback::onContact(
    const PxContactPairHeader& PairHeader,
    const PxContactPair* Pairs,
    PxU32 NbPairs)
{
    // Actor가 삭제 중인지 확인
    if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0 ||
        PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
    {
        return;  // 삭제된 액터면 무시
    }

    // userData null 체크
    FBodyInstance* Body0 = FPhysicsUserData::Get(PairHeader.actors[0]->userData);
    FBodyInstance* Body1 = FPhysicsUserData::Get(PairHeader.actors[1]->userData);

    if (!Body0 || !Body1) return;
    if (!Body0->OwnerComponent || !Body1->OwnerComponent) return;

    // ... 기존 충돌 처리 로직
}
```

---

### 5단계: 시뮬레이션 중 수정 요청 지연 처리

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysSceneImpl.h`

```cpp
// 지연 명령 큐 추가
struct FPhysicsCommand
{
    enum class EType { AddActor, RemoveActor, SetTransform };
    EType Type;
    physx::PxActor* Actor;
    FTransform Transform;
};

TArray<FPhysicsCommand> PendingCommands;
bool bIsSimulating = false;
```

**파일**: `Mundi/Source/Runtime/Engine/Physics/PhysScene.cpp`

```cpp
void FPhysSceneImpl::AddActor(physx::PxActor* Actor)
{
    if (bIsSimulating)
    {
        // 시뮬레이션 중이면 지연 처리
        PendingCommands.Add({FPhysicsCommand::EType::AddActor, Actor, {}});
        return;
    }

    SCOPED_WRITE_LOCK(PScene);
    PScene->addActor(*Actor);
}

void FPhysSceneImpl::FetchResults()
{
    // ... fetchResults 완료 후

    // 지연된 명령 처리
    ProcessPendingCommands();
}

void FPhysSceneImpl::ProcessPendingCommands()
{
    SCOPED_WRITE_LOCK(PScene);

    for (const auto& Cmd : PendingCommands)
    {
        switch (Cmd.Type)
        {
        case FPhysicsCommand::EType::AddActor:
            PScene->addActor(*Cmd.Actor);
            break;
        case FPhysicsCommand::EType::RemoveActor:
            PScene->removeActor(*Cmd.Actor);
            break;
        // ...
        }
    }
    PendingCommands.Empty();
}
```

---

## 수정 대상 파일 목록

| 파일 | 작업 |
|------|------|
| `Physics/PhysicsSceneLock.h` | 신규 생성 - RAII Lock 래퍼 |
| `Physics/PhysSceneImpl.h` | FPhysicsCommand, PendingCommands 추가 |
| `Physics/PhysScene.cpp` | Lock 적용, 지연 명령 처리 |
| `Physics/BodyInstance.cpp` | Transform 접근에 Lock 적용 |
| `Physics/PhysicsEventCallback.cpp` | 삭제 액터 플래그 체크 추가 |

---

## 구현 순서

1. `PhysicsSceneLock.h` 생성 (RAII Lock 래퍼)
2. `PhysSceneImpl.h`에 지연 명령 큐 구조 추가
3. `PhysScene.cpp`에 Lock 및 지연 처리 적용
4. `BodyInstance.cpp`에 Lock 적용
5. `PhysicsEventCallback.cpp` 안전성 강화
6. 빌드 및 테스트

---

## 언리얼 엔진 참고 파일

| 언리얼 엔진 파일 | 참고 내용 |
|-----------------|----------|
| `Runtime/Experimental/Chaos/Public/Framework/Threading.h` | Lock 클래스 전체 구현 |
| `Runtime/Engine/Public/Physics/Experimental/ChaosScopedSceneLock.h` | FScopedSceneLock_Chaos 패턴 |
| `Runtime/Engine/Private/PhysicsEngine/PhysSubstepTasks.h` | `_AssumesLocked` 함수 패턴 |
