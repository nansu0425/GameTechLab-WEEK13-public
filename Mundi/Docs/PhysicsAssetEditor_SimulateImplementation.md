# Physics Asset Editor Simulate 기능 구현 계획

## 개요

Physics Asset Editor에서 Simulate 버튼 클릭 시 래그돌 물리 시뮬레이션을 실행하는 기능 구현 계획입니다.

## 현재 구조 분석

### PIE 모드 물리 시뮬레이션 흐름
```
World::BeginPlay()
    → Actor::BeginPlay()
        → UPrimitiveComponent::BeginPlay()
            → CreatePhysicsState()
                → FPhysScene::AddActor()

World::Tick()
    → FPhysScene::Tick()
        → PxScene::simulate()
        → PxScene::fetchResults()
```

### 현재 Physics Asset Editor 구조

| 항목 | 현재 상태 | 문제점 |
|------|-----------|--------|
| PreviewWorld | `IsPreviewWorld() == true` | FPhysScene 미생성 |
| World::Tick() | 호출됨 | FPhysScene::Tick() 미호출 |
| StartSimulation() | SetSimulatePhysics(true) 호출 | CreatePhysicsState() 미호출 |
| UpdateBoneTransforms | 미호출 | 본 트랜스폼 업데이트 안됨 |

### 관련 파일
- `Source/Slate/Windows/SPhysicsAssetEditorWindow.cpp` - 에디터 UI 및 시뮬레이션 제어
- `Source/Runtime/Engine/Level/World.cpp` - 월드 틱 및 물리 씬 관리
- `Source/Runtime/Engine/Components/SkeletalMeshComponent.cpp` - 스켈레탈 메시 물리

## 구현 계획

### Step 1: PreviewWorld에 FPhysScene 생성

**파일**: `World.cpp`

현재 `IsPreviewWorld()` 체크로 인해 FPhysScene이 생성되지 않습니다.

```cpp
// 현재 코드 (문제)
if (!IsPreviewWorld())
{
    PhysScene = new FPhysScene();
}

// 수정 방안
// PreviewWorld도 FPhysScene 생성 가능하도록 변경
PhysScene = new FPhysScene();
```

또는 별도의 플래그로 제어:
```cpp
bool bEnablePhysicsInPreview = false;

void World::EnablePhysicsSimulation(bool bEnable)
{
    bEnablePhysicsInPreview = bEnable;
    if (bEnable && !PhysScene)
    {
        PhysScene = new FPhysScene();
    }
}
```

### Step 2: World::Tick()에서 Physics Tick 호출

**파일**: `World.cpp`

```cpp
void World::Tick(float DeltaTime)
{
    // 기존 로직...

    // Physics 시뮬레이션 (PreviewWorld 포함)
    if (PhysScene && bSimulatingPhysics)
    {
        PhysScene->Tick(DeltaTime);
    }

    // 기존 로직...
}
```

### Step 3: StartSimulation() 강화

**파일**: `SPhysicsAssetEditorWindow.cpp`

```cpp
void SPhysicsAssetEditorWindow::StartSimulation()
{
    if (!ActiveState || !ActiveState->PreviewActor)
        return;

    UWorld* PreviewWorld = ActiveState->PreviewWorld;
    if (!PreviewWorld)
        return;

    // 1. PreviewWorld의 Physics 활성화
    PreviewWorld->EnablePhysicsSimulation(true);

    // 2. SkeletalMeshComponent의 물리 상태 생성
    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // 물리 상태 생성 (Bodies + Constraints)
        MeshComp->CreatePhysicsState();

        // 시뮬레이션 활성화
        MeshComp->SetSimulatePhysics(true);
    }

    bIsSimulating = true;
}
```

### Step 4: 매 프레임 본 트랜스폼 업데이트

**파일**: `SPhysicsAssetEditorWindow.cpp` (RenderPreview 또는 별도 Tick)

```cpp
void SPhysicsAssetEditorWindow::RenderPreview()
{
    // 기존 렌더링 로직...

    // 시뮬레이션 중이면 본 트랜스폼 업데이트
    if (bIsSimulating && ActiveState && ActiveState->PreviewActor)
    {
        if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            MeshComp->UpdateBoneTransformsFromPhysics();
        }
    }
}
```

### Step 5: StopSimulation() 강화

**파일**: `SPhysicsAssetEditorWindow.cpp`

```cpp
void SPhysicsAssetEditorWindow::StopSimulation()
{
    if (!ActiveState || !ActiveState->PreviewActor)
        return;

    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // 1. 시뮬레이션 비활성화
        MeshComp->SetSimulatePhysics(false);

        // 2. 물리 상태 정리
        MeshComp->DestroyPhysicsState();

        // 3. 원래 포즈로 복원
        MeshComp->SetComponentToWorld(OriginalTransform);
        MeshComp->RecreatePhysicsState(); // 선택적
    }

    UWorld* PreviewWorld = ActiveState->PreviewWorld;
    if (PreviewWorld)
    {
        PreviewWorld->EnablePhysicsSimulation(false);
    }

    bIsSimulating = false;
}
```

## 추가 고려사항

### 바닥 충돌체 (Floor Collision)

시뮬레이션 테스트를 위해 바닥 충돌체가 필요합니다:

```cpp
void SPhysicsAssetEditorWindow::CreateFloorCollision()
{
    // 정적 평면 또는 박스 액터 생성
    // Physics Asset Editor 프리뷰에 바닥 추가
}
```

### PhysX PVD 연동

PreviewWorld의 FPhysScene도 PVD에 연결되어 디버깅 가능하도록 설정.

---

## 관련 작업: CalcPxTransform PhysX 좌표계 수정

### 문제 상황

Physics Asset Editor와 PVD에서 constraint 원뿔 방향이 90도 어긋나는 현상 발생.

### 원인

CalcPxTransform이 Mundi 좌표계에서 축을 계산한 후 ToPxQuat로 변환하는 과정에서 축 매핑 불일치 발생.

### 해결책

PhysX 좌표계에서 직접 축을 계산하도록 변경:

**파일**: `Source/Runtime/Engine/Physics/ConstraintInstance.cpp`

```cpp
physx::PxTransform FConstraintInstance::CalcPxTransform(
    const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis)
{
    using namespace PhysicsConversion;

    // PhysX D6 Joint 축 규약:
    //   PhysX X축 = Twist 축 (회전축, 원뿔 중심축)
    //   PhysX Y축 = Swing1 방향
    //   PhysX Z축 = Swing2 방향

    // 1. 위치 변환: Mundi → PhysX 좌표계
    physx::PxVec3 PxPos = ToPxVec3(Pos);

    // 2. PriAxis(Twist)와 SecAxis를 PhysX 좌표계로 변환
    physx::PxVec3 PxTwist = ToPxVec3(PriAxis).getNormalized();
    physx::PxVec3 PxSecAxis = ToPxVec3(SecAxis).getNormalized();

    // 3. PhysX 좌표계에서 직교 좌표계 구성 (Right-Handed)
    physx::PxVec3 PxZAxis = PxTwist.cross(PxSecAxis);
    if (PxZAxis.magnitudeSquared() < 1e-6f)
    {
        physx::PxVec3 Fallback = fabsf(PxTwist.y) < 0.99f
            ? physx::PxVec3(0.0f, 1.0f, 0.0f)
            : physx::PxVec3(1.0f, 0.0f, 0.0f);
        PxZAxis = PxTwist.cross(Fallback);
    }
    PxZAxis.normalize();

    physx::PxVec3 PxYAxis = PxZAxis.cross(PxTwist);
    PxYAxis.normalize();

    // 4. PhysX 회전 행렬 생성 (열 기준)
    physx::PxMat33 PxRotMat(PxTwist, PxYAxis, PxZAxis);
    physx::PxQuat PxRot(PxRotMat);
    PxRot.normalize();

    // 5. AngularRotationOffset 적용
    if (!AngularRotationOffset.IsIdentity())
    {
        physx::PxQuat PxOffset = ToPxQuat(AngularRotationOffset);
        PxRot = PxRot * PxOffset;
        PxRot.normalize();
    }

    return physx::PxTransform(PxPos, PxRot);
}
```

---

## 작업 순서 요약

1. ✅ CalcPxTransform PhysX 좌표계 직접 계산 수정 완료
2. ⬜ World.cpp - PreviewWorld FPhysScene 생성 로직 추가
3. ⬜ World.cpp - Physics Tick 호출 추가
4. ⬜ SPhysicsAssetEditorWindow.cpp - StartSimulation() 강화
5. ⬜ SPhysicsAssetEditorWindow.cpp - 본 트랜스폼 업데이트 추가
6. ⬜ SPhysicsAssetEditorWindow.cpp - StopSimulation() 강화
7. ⬜ (선택) 바닥 충돌체 추가
