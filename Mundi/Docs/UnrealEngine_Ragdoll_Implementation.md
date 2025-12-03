# 언리얼 엔진 래그돌 구현 분석

이 문서는 언리얼 엔진의 래그돌 물리 시뮬레이션 구현을 분석한 내용입니다.
Mundi 엔진의 래그돌 구현 시 참고용으로 작성되었습니다.

---

## 1. 개요

언리얼 엔진의 래그돌은 **SkeletalMeshComponent**와 **PhysicsAsset**을 기반으로 구현됩니다.

핵심은 본(Bone) 트랜스폼과 물리 엔진 간의 양방향 동기화입니다:
- **애니메이션 → 물리**: 본 트랜스폼을 Kinematic Body로 업데이트
- **물리 → 애니메이션**: 시뮬레이션 결과를 본 트랜스폼에 블렌딩

---

## 2. 좌표계 구조

```
WorldSpace (월드 좌표계)
    ↓ GetRelativeTransform(ComponentTransform)
ComponentSpace (컴포넌트 로컬 좌표)
    ↓ GetRelativeTransform(ParentBoneTransform)
BoneSpace (본 로컬 좌표)
```

### 2.1 변환 공식

```cpp
// BoneSpace → WorldSpace
FTransform BoneWorldTM = ComponentSpaceBones[BoneIndex] * ComponentToWorld;

// WorldSpace → BoneSpace (역변환)
FTransform ParentWorldTM = WorldBoneTMs[ParentIndex];
FTransform RelativeTM = PhysWorldTM.GetRelativeTransform(ParentWorldTM);
```

---

## 3. 핵심 함수들

### 3.1 애니메이션 → 물리 (`UpdateKinematicBonesToAnim`)

**파일**: `PhysicsEngine/PhysAnim.cpp:538`

```cpp
void USkeletalMeshComponent::UpdateKinematicBonesToAnim(
    const TArray<FTransform>& InSpaceBases,  // ComponentSpace 본 트랜스폼
    ETeleportType Teleport,
    bool bNeedsSkinning,
    EAllowKinematicDeferral DeferralAllowed)
{
    FTransform CurrentLocalToWorld = GetComponentTransform();

    for (int32 i = 0; i < NumBodies; i++)
    {
        FBodyInstance* BodyInst = Bodies[i];

        // Simulated Body는 스킵 (물리 엔진이 제어)
        if (BodyInst->IsInstanceSimulatingPhysics() && !bTeleport)
            continue;

        int32 BoneIndex = BodyInst->InstanceBoneIndex;

        // ComponentSpace → WorldSpace 변환
        const FTransform BoneTransform = InSpaceBases[BoneIndex] * CurrentLocalToWorld;

        // Kinematic Body 위치 설정
        PhysScene->SetKinematicTarget_AssumesLocked(BodyInst, BoneTransform, true);
    }
}
```

### 3.2 물리 → 애니메이션 (`PerformBlendPhysicsBones`)

**파일**: `PhysicsEngine/PhysAnim.cpp:145`

```cpp
void USkeletalMeshComponent::PerformBlendPhysicsBones(
    const TArray<FBoneIndexType>& InRequiredBones,
    TArray<FTransform>& InOutComponentSpaceTransforms,
    TArray<FTransform>& InOutBoneSpaceTransforms)
{
    FTransform LocalToWorldTM = GetComponentTransform();

    for (int32 BoneIndex : RequiredBones)
    {
        FBodyInstance* BodyInst = GetBodyInstance(BoneIndex);
        if (!BodyInst) continue;

        // 1. 물리 결과 읽기 (WorldSpace)
        FTransform PhysTM = BodyInst->GetUnrealWorldTransform_AssumesLocked();

        // 2. 부모 월드 트랜스폼 구하기
        int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        FTransform ParentWorldTM = (ParentIndex == INDEX_NONE)
            ? LocalToWorldTM
            : WorldBoneTMs[ParentIndex];

        // 3. 상대 트랜스폼 계산 (WorldSpace → BoneSpace)
        FTransform RelTM = PhysTM.GetRelativeTransform(ParentWorldTM);
        RelTM.RemoveScaling();  // 스케일 제거

        // 4. 본 로컬 트랜스폼 생성 (원본 스케일 유지!)
        FTransform PhysicalBoneSpaceTransform(
            RelTM.GetRotation(),
            RelTM.GetLocation(),
            InOutBoneSpaceTransforms[BoneIndex].GetScale3D()  // 원본 스케일 유지
        );

        // 5. 블렌딩 (PhysicsBlendWeight 적용)
        float Weight = BodyInst->PhysicsBlendWeight;
        InOutBoneSpaceTransforms[BoneIndex].Blend(
            InOutBoneSpaceTransforms[BoneIndex],  // 애니메이션 트랜스폼
            PhysicalBoneSpaceTransform,           // 물리 트랜스폼
            Weight
        );

        // 6. ComponentSpace 재계산
        InOutComponentSpaceTransforms[BoneIndex] =
            InOutBoneSpaceTransforms[BoneIndex] * InOutComponentSpaceTransforms[ParentIndex];
    }
}
```

---

## 4. Mundi 엔진에 적용할 핵심 포인트

### 4.1 물리 → 본 트랜스폼 동기화 (가장 중요!)

**현재 Mundi의 문제점**: `GetBoneWorldTransform()`과 `CurrentComponentSpacePose`의 스케일 불일치

**언리얼의 해결책**:
1. 물리 바디에서 월드 트랜스폼 읽기
2. **부모 본의 월드 트랜스폼으로 상대 변환**
3. **스케일 제거 후 원본 스케일 복원**

```cpp
// Mundi에 적용할 코드
void USkeletalMeshComponent::UpdateBoneTransformsFromPhysics()
{
    FTransform ComponentToWorld = GetWorldTransform();

    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        FBodyInstance* Body = Bodies[i];
        int32 BoneIndex = /* Body에 대응하는 본 인덱스 */;

        // 1. 물리 바디의 월드 트랜스폼
        FTransform BodyWorldTM = Body->GetWorldTransform();

        // 2. 부모 본의 월드 트랜스폼 계산
        int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        FTransform ParentWorldTM;
        if (ParentIndex == -1)
        {
            ParentWorldTM = ComponentToWorld;
        }
        else
        {
            // 부모 본의 ComponentSpace → WorldSpace
            ParentWorldTM = ComponentToWorld.GetWorldTransform(CurrentComponentSpacePose[ParentIndex]);
        }

        // 3. 상대 트랜스폼 계산 (핵심!)
        FTransform RelativeTM = ParentWorldTM.GetRelativeTransform(BodyWorldTM);

        // 4. 스케일 유지 (원본 본 스케일 사용!)
        FTransform NewBoneLocal(
            RelativeTM.Rotation,
            RelativeTM.Translation,
            CurrentLocalSpacePose[BoneIndex].Scale3D  // 원본 스케일 유지
        );

        // 5. 본 트랜스폼 업데이트
        CurrentLocalSpacePose[BoneIndex] = NewBoneLocal;
    }

    // 6. ComponentSpace 재계산
    UpdateComponentSpaceTransforms();
    UpdateFinalSkinningMatrices();
}
```

### 4.2 Constraint 초기화

**언리얼의 방식**:
- Pos1/Pos2는 **각 Rigid Body의 로컬 좌표계** 기준
- PhysicsAsset에서 미리 계산된 값 사용
- 스케일은 `LastKnownScale`로 별도 관리

```cpp
// Constraint 참조 프레임
FTransform GetRefFrame(EConstraintFrame::Type Frame)
{
    if (Frame == EConstraintFrame::Frame1)
        return FTransform(PriAxis1, SecAxis1, PriAxis1 ^ SecAxis1, Pos1);
    else
        return FTransform(PriAxis2, SecAxis2, PriAxis2 ^ SecAxis2, Pos2);
}

// Constraint 생성 시 스케일 적용
Local1.ScaleTranslation(FVector(LastKnownScale));
Local2.ScaleTranslation(FVector(LastKnownScale));
```

---

## 5. 주요 차이점 및 주의사항

### 5.1 좌표 공간 일관성

| 항목 | 언리얼 | Mundi |
|------|--------|-------|
| 본 트랜스폼 | BoneSpace (로컬) | CurrentLocalSpacePose |
| 물리 바디 | WorldSpace | WorldSpace |
| Constraint Pos | Body 로컬 공간 | Body 로컬 공간 |

### 5.2 스케일 처리

**언리얼**:
- 물리 → 본 변환 시 **스케일 제거** (`RemoveScaling()`)
- 본 트랜스폼에 **원본 스케일 복원**
- Constraint에 **LastKnownScale** 별도 적용

**Mundi에서 필요한 처리**:
```cpp
// 물리 트랜스폼에서 스케일 제거
FTransform RelativeTM = ParentWorldTM.GetRelativeTransform(BodyWorldTM);
// RelativeTM에서 Scale3D는 무시하고 원본 사용

// 원본 스케일 복원 (이것이 100배 스케일 문제의 해결책!)
NewBoneLocal.Scale3D = CurrentLocalSpacePose[BoneIndex].Scale3D;
```

### 5.3 Kinematic vs Simulated

```cpp
// 본 상태에 따른 처리 분기
if (Body->IsInstanceSimulatingPhysics())
{
    // Simulated: 물리 결과를 본에 적용
    UpdateBoneFromPhysics(BoneIndex);
}
else
{
    // Kinematic: 애니메이션 결과를 물리에 적용
    UpdatePhysicsFromBone(BoneIndex);
}
```

---

## 6. Mundi 구현 체크리스트

### 6.1 물리 → 본 동기화 (UpdateBoneTransformsFromPhysics)

- [ ] 물리 바디 월드 트랜스폼 읽기
- [ ] 부모 본 월드 트랜스폼 계산
- [ ] **GetRelativeTransform으로 상대 변환** (핵심!)
- [ ] **원본 스케일 유지** (100배 스케일 문제 해결)
- [ ] ComponentSpace 재계산
- [ ] 스키닝 행렬 업데이트

### 6.2 Constraint 초기화 (InitializeConstraints)

- [ ] Pos1: Child Body 로컬 공간에서 조인트 위치
- [ ] Pos2: Parent Body 로컬 공간에서 조인트 위치
- [ ] **동일한 스케일 공간에서 계산** (CurrentComponentSpacePose 직접 사용)
- [ ] 축(PriAxis, SecAxis) 설정

### 6.3 스케일 일관성 검증

- [ ] BodySetup.SphylElems.Center 스케일 확인
- [ ] CurrentComponentSpacePose 스케일 확인
- [ ] GetBoneWorldTransform 스케일 확인
- [ ] Constraint Pos1/Pos2 스케일 일치 확인

---

## 7. 디버깅 팁

### 7.1 로그 추가 위치

```cpp
// 물리 → 본 동기화 디버깅
UE_LOG("[Ragdoll] Body[%d] WorldPos=(%.2f,%.2f,%.2f)",
    i, BodyWorldTM.Translation.X, BodyWorldTM.Translation.Y, BodyWorldTM.Translation.Z);
UE_LOG("[Ragdoll] ParentWorldPos=(%.2f,%.2f,%.2f)",
    ParentWorldTM.Translation.X, ParentWorldTM.Translation.Y, ParentWorldTM.Translation.Z);
UE_LOG("[Ragdoll] RelativeTM=(%.4f,%.4f,%.4f) Scale=(%.4f,%.4f,%.4f)",
    RelativeTM.Translation.X, RelativeTM.Translation.Y, RelativeTM.Translation.Z,
    RelativeTM.Scale3D.X, RelativeTM.Scale3D.Y, RelativeTM.Scale3D.Z);
```

### 7.2 시각적 디버깅

- 각 본 위치에 디버그 구체 그리기
- Constraint 위치에 디버그 선 그리기
- Body 월드 위치와 본 월드 위치 비교

---

## 8. PhysicsAsset 자동 생성 (RigidBody & Constraint)

언리얼 엔진은 SkeletalMesh로부터 PhysicsAsset을 자동으로 생성하는 기능을 제공합니다.

**파일**: `Developer/PhysicsUtilities/Private/PhysicsAssetUtils.cpp`

### 8.1 전체 생성 흐름

```
CreateFromSkeletalMesh()
  │
  ├─ 1. 각 본별 정점(Vertex) 정보 수집
  │     └─ CalcBoneVertInfos()
  │
  ├─ 2. 본 크기 계산 및 병합 결정
  │     ├─ CalcBoneInfoLength() → BoundingBox 크기
  │     └─ MinBoneSize보다 작으면 부모와 병합
  │
  ├─ 3. 각 본별 Body 생성
  │     ├─ CreateNewBody()
  │     └─ CreateCollisionFromBoneInternal() → Shape 계산
  │
  └─ 4. Constraint 자동 생성
        ├─ 부모 Body 찾기
        ├─ CreateNewConstraint()
        └─ SnapTransformsToDefault() → Pos1/Pos2 계산
```

### 8.2 RigidBody(BodySetup) 자동 생성

#### 8.2.1 본 크기 계산

```cpp
static float CalcBoneInfoLength(const FBoneVertInfo& Info)
{
    // 본에 연결된 정점들로 BoundingBox 계산
    FBox BoneBox(ForceInit);
    for (int32 j = 0; j < Info.Positions.Num(); j++)
    {
        BoneBox += (FVector)Info.Positions[j];
    }

    if (BoneBox.IsValid)
    {
        FVector BoxExtent = BoneBox.GetExtent();
        return BoxExtent.Size();  // 대각선 길이
    }
    return 0.f;
}
```

#### 8.2.2 본 병합 전략

작은 본들은 자동으로 부모와 병합됩니다 (손가락, 발가락 등):

```cpp
// 자식에서 부모로 역순 순회
for (int32 BoneIdx = NumBones - 1; BoneIdx >= 0; --BoneIdx)
{
    float MyMergedSize = MergedSizes[BoneIdx] += CalcBoneInfoLength(Infos[BoneIdx]);

    // 본이 MinBoneSize보다 작으면 부모와 병합
    if (MyMergedSize < Params.MinBoneSize && MyMergedSize >= Params.MinWeldSize)
    {
        int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIdx);
        if (ParentIndex != INDEX_NONE)
        {
            // 부모에 현재 본 크기 누적
            MergedSizes[ParentIndex] += MyMergedSize;
        }
    }
}
```

#### 8.2.3 Body 생성 조건

```cpp
auto ShouldMakeBone = [](int32 BoneIndex) {
    // 1. 모든 본에 Body 생성 옵션
    if (Params.bBodyForAll) return true;

    // 2. MinBoneSize보다 큰 본
    if (MergedSizes[BoneIndex] > Params.MinBoneSize) return true;

    // 3. 강제 루트 본
    if (BoneIndex == ForcedRootBoneIndex) return true;

    return false;
};
```

### 8.3 충돌 Shape 자동 계산

#### 8.3.1 Shape 방향 결정 (공분산 행렬)

```cpp
if (Params.bAutoOrientToBone)
{
    // 정점들의 공분산 행렬 계산
    const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);

    // Power Method로 주 고유벡터(가장 긴 축) 추출
    FVector ZAxis = ComputeEigenVector(CovarianceMatrix);

    // Z축에 수직인 X, Y축 계산
    FVector XAxis, YAxis;
    ZAxis.FindBestAxisVectors(YAxis, XAxis);

    ElemTM = FMatrix(XAxis, YAxis, ZAxis, FVector::ZeroVector);
}
```

#### 8.3.2 캡슐(Sphyl) 계산 공식

```cpp
// BoundingBox의 가장 긴 축을 캡슐 길이로 사용
FVector BoxExtent = BoneBox.GetExtent();

if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
{
    // X축이 가장 김 → X를 길이 축으로
    SphylElem.SetTransform(FTransform(FQuat(FVector(0,1,0), -PI*0.5f)) * ElementTransform);
    SphylElem.Radius = FMath::Max(BoxExtent.Y, BoxExtent.Z) * 1.01f;
    SphylElem.Length = BoxExtent.X * 1.01f;
}
else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
{
    // Y축이 가장 김
    SphylElem.SetTransform(FTransform(FQuat(FVector(1,0,0), PI*0.5f)) * ElementTransform);
    SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Z) * 1.01f;
    SphylElem.Length = BoxExtent.Y * 1.01f;
}
else
{
    // Z축이 가장 김 (기본)
    SphylElem.SetTransform(ElementTransform);
    SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
    SphylElem.Length = BoxExtent.Z * 1.01f;
}

// Center = ElementTransform의 Translation (BoundingBox 중심)
```

**캡슐 계산 요약:**

| 최장 축 | Length | Radius | 회전 |
|---------|--------|--------|------|
| X | BoxExtent.X × 1.01 | max(Y, Z) × 1.01 | Y축 -90° |
| Y | BoxExtent.Y × 1.01 | max(X, Z) × 1.01 | X축 +90° |
| Z | BoxExtent.Z × 1.01 | max(X, Y) × 1.01 | Identity |

> **1.01 배수**: 그래픽 클리핑 방지용 여유분

### 8.4 Constraint 자동 생성

#### 8.4.1 부모 Body 찾기

```cpp
int32 ParentBodyIndex = INDEX_NONE;
int32 ParentIndex = BoneIndex;

// 부모 본 계층을 따라올라가며 Body를 가진 부모 찾기
do {
    ParentIndex = RefSkeleton.GetParentIndex(ParentIndex);
    if (ParentIndex != INDEX_NONE)
    {
        FName ParentName = RefSkeleton.GetBoneName(ParentIndex);
        ParentBodyIndex = PhysicsAsset->FindBodyIndex(ParentName);
    }
} while (ParentBodyIndex == INDEX_NONE && ParentIndex != INDEX_NONE);
```

#### 8.4.2 Pos1, Pos2 계산 (핵심!)

**파일**: `PhysicsEngine/ConstraintInstance.cpp`

```cpp
void FConstraintInstance::SnapTransformsToDefault(
    EConstraintTransformComponentFlags SnapFlags,
    const UPhysicsAsset* PhysicsAsset)
{
    // Frame1 (자식): 항상 자신의 원점
    FTransform ChildTransform = FTransform::Identity;
    SetRefPosition(EConstraintFrame::Frame1, FVector::Zero());  // Pos1 = (0,0,0)

    // Frame2 (부모): 부모 본 기준 자식 본의 상대 위치
    FTransform ParentTransform = CalculateDefaultParentTransform(PhysicsAsset);
    SetRefPosition(EConstraintFrame::Frame2, ParentTransform.GetLocation());  // Pos2
}
```

**CalculateRelativeBoneTransform - Pos2 계산:**

```cpp
FTransform CalculateRelativeBoneTransform(
    FName ToBoneName,    // 자식
    FName FromBoneName,  // 부모
    const FReferenceSkeleton& RefSkeleton)
{
    const TArray<FTransform>& LocalPose = RefSkeleton.GetRefBonePose();

    int32 ToIndex = RefSkeleton.FindBoneIndex(ToBoneName);
    int32 FromIndex = RefSkeleton.FindBoneIndex(FromBoneName);

    FTransform ToCommon = FTransform::Identity;
    FTransform FromCommon = FTransform::Identity;

    // 공통 조상을 찾을 때까지 계층 구조를 올라감
    while (ToIndex != FromIndex)
    {
        if (ToIndex > FromIndex)
        {
            // 자식이 더 깊음 → 자식 방향으로 Transform 누적
            ToCommon = ToCommon * LocalPose[ToIndex];
            ToIndex = RefSkeleton.GetParentIndex(ToIndex);
        }
        else
        {
            // 부모가 더 깊음
            FromCommon = FromCommon * LocalPose[FromIndex];
            FromIndex = RefSkeleton.GetParentIndex(FromIndex);
        }
    }

    // Pos2 = 부모 본 로컬 좌표에서 자식 본의 위치
    return ToCommon.GetRelativeTransform(FromCommon);
}
```

#### 8.4.3 Pos1/Pos2 계산 예시

```
스켈레톤:
  Arm_L (부모)
    └─ Forearm_L (자식)

Constraint: Arm_L ↔ Forearm_L
```

| 항목 | 값 | 설명 |
|------|-----|------|
| **Pos1** | `(0, 0, 0)` | 자식(Forearm_L) Body의 로컬 원점 |
| **Pos2** | `LocalPose[Forearm_L].Translation` | 부모(Arm_L) 로컬 좌표에서 자식 위치 |

예: `Pos2 = (0, 0, 30)` → Arm_L의 Z축으로 30 단위 떨어진 곳

#### 8.4.4 축 방향 설정

```cpp
// Frame1 (자식): Identity의 축
PriAxis1 = (1, 0, 0);  // X축 = Twist axis
SecAxis1 = (0, 1, 0);  // Y축 = Swing axis

// Frame2 (부모): 상대 Transform의 축
PriAxis2 = ParentTransform.GetUnitAxis(EAxis::X);
SecAxis2 = ParentTransform.GetUnitAxis(EAxis::Y);
```

### 8.5 생성 파라미터 (FPhysAssetCreateParams)

```cpp
struct FPhysAssetCreateParams
{
    // Body 생성
    float MinBoneSize = 20.0f;      // 이보다 작은 본은 무시/병합
    float MinWeldSize = 0.001f;     // 병합 최소 크기
    bool bBodyForAll = false;       // 모든 본에 Body 생성

    // Shape
    EPhysAssetFitGeomType GeomType = EFG_Sphyl;  // 캡슐
    bool bAutoOrientToBone = true;  // 자동 방향 계산

    // Constraint
    bool bCreateConstraints = true;
    EAngularConstraintMotion AngularConstraintMode = ACM_Limited;

    // 충돌
    bool bDisableCollisionsByDefault = true;
};
```

### 8.6 Mundi 구현 시 적용 포인트

#### Pos1/Pos2 계산 (현재 문제의 핵심)

```cpp
// 언리얼 방식 적용
void InitializeConstraints()
{
    for (각 BodySetup)
    {
        // Pos1: 자식 Body의 원점 (본 원점 - 캡슐 중심)
        NewJoint->Pos1 = -ChildCapsuleLocalCenter;

        // Pos2: 부모 로컬에서 자식 본 위치
        // 핵심: RefPose(로컬 포즈)에서 직접 계산!
        FTransform ChildLocalPose = RefPose[ChildBoneIndex];
        NewJoint->Pos2 = ChildLocalPose.Translation - ParentCapsuleLocalCenter;

        // 축 설정
        NewJoint->PriAxis1 = FVector(1, 0, 0);
        NewJoint->SecAxis1 = FVector(0, 1, 0);
        NewJoint->PriAxis2 = ChildLocalPose.Rotation.RotateVector(FVector(1, 0, 0));
        NewJoint->SecAxis2 = ChildLocalPose.Rotation.RotateVector(FVector(0, 1, 0));
    }
}
```

**핵심**: `CurrentComponentSpacePose`나 `GetBoneWorldTransform()` 대신 **`RefPose`(본 로컬 포즈)**를 직접 사용하면 스케일 문제를 피할 수 있습니다.

---

## 9. 참고 파일 (언리얼 엔진)

| 파일 | 내용 |
|------|------|
| `PhysicsEngine/PhysAnim.cpp` | 물리↔애니메이션 동기화 |
| `PhysicsEngine/ConstraintInstance.cpp` | Constraint 초기화/관리, Pos1/Pos2 계산 |
| `Components/SkeletalMeshComponentPhysics.cpp` | 물리 상태 생성/파괴 |
| `PhysicsEngine/BodyInstance.cpp` | Body 인스턴스 관리 |
| `Developer/PhysicsUtilities/PhysicsAssetUtils.cpp` | **PhysicsAsset 자동 생성** |

---

## 10. 요약

**래그돌 구현의 핵심은 좌표 공간 변환의 일관성입니다.**

1. **물리 → 본**: `GetRelativeTransform(ParentWorldTM)`으로 로컬 변환 계산
2. **스케일 처리**: 물리에서 읽은 스케일 무시, **원본 본 스케일 유지**
3. **Constraint**: 동일 스케일 공간에서 Pos1/Pos2 계산

이 세 가지 원칙을 지키면 스케일 불일치로 인한 폭발 문제를 방지할 수 있습니다.
