#pragma once

#include <PxPhysicsAPI.h>

/**
 * PhysX Scene Lock 래퍼 클래스
 *
 * PhysX의 PxScene::lockRead()/lockWrite()를 RAII 패턴으로 래핑하여
 * 멀티스레드 환경에서 안전한 물리 씬 접근을 보장합니다.
 *
 * 언리얼 엔진의 Chaos Physics Threading.h를 참고하여 구현되었습니다.
 */

/**
 * RAII 읽기 잠금
 * 여러 스레드가 동시에 읽기 잠금을 획득할 수 있습니다.
 * Transform 조회 등 읽기 전용 작업에 사용합니다.
 */
class FPhysSceneReadLock
{
public:
    explicit FPhysSceneReadLock(physx::PxScene* InScene)
        : Scene(InScene)
    {
        if (Scene)
        {
            Scene->lockRead();
        }
    }

    ~FPhysSceneReadLock()
    {
        if (Scene)
        {
            Scene->unlockRead();
        }
    }

    // 복사/이동 금지
    FPhysSceneReadLock(const FPhysSceneReadLock&) = delete;
    FPhysSceneReadLock& operator=(const FPhysSceneReadLock&) = delete;
    FPhysSceneReadLock(FPhysSceneReadLock&&) = delete;
    FPhysSceneReadLock& operator=(FPhysSceneReadLock&&) = delete;

private:
    physx::PxScene* Scene;
};

/**
 * RAII 쓰기 잠금
 * 한 번에 하나의 스레드만 쓰기 잠금을 획득할 수 있습니다.
 * Actor 추가/제거, Transform 설정, Simulate 등 씬 수정 작업에 사용합니다.
 */
class FPhysSceneWriteLock
{
public:
    explicit FPhysSceneWriteLock(physx::PxScene* InScene)
        : Scene(InScene)
    {
        if (Scene)
        {
            Scene->lockWrite();
        }
    }

    ~FPhysSceneWriteLock()
    {
        if (Scene)
        {
            Scene->unlockWrite();
        }
    }

    // 복사/이동 금지
    FPhysSceneWriteLock(const FPhysSceneWriteLock&) = delete;
    FPhysSceneWriteLock& operator=(const FPhysSceneWriteLock&) = delete;
    FPhysSceneWriteLock(FPhysSceneWriteLock&&) = delete;
    FPhysSceneWriteLock& operator=(FPhysSceneWriteLock&&) = delete;

private:
    physx::PxScene* Scene;
};

/**
 * 편의 매크로
 *
 * 사용 예:
 *   SCOPED_SCENE_READ_LOCK(PScene);
 *   // 읽기 작업...
 *
 *   SCOPED_SCENE_WRITE_LOCK(PScene);
 *   // 쓰기 작업...
 */
#define SCOPED_SCENE_READ_LOCK(Scene) FPhysSceneReadLock ScopedReadLock##__LINE__(Scene)
#define SCOPED_SCENE_WRITE_LOCK(Scene) FPhysSceneWriteLock ScopedWriteLock##__LINE__(Scene)
