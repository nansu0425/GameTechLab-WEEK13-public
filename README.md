# GameTechLab WEEK13 — PhysX 통합 · 물리 멀티스레딩

KRAFTON 정글 게임테크랩 2기 WEEK13 저장소에서 **소스 코드와 본인 작성 문서만 추출한 공개용 저장소** 입니다. 원본은 팀 저장소(비공개)이고, 이 저장소는 포트폴리오 목적으로 공개하기 위한 것입니다.

WEEK13 과제 주제는 *두 개의 DOF — Depth of Field 와 Degrees of Freedom* 이었고, 본인 담당은 **PhysX 통합과 물리 씬의 멀티스레딩**, **Depth of Field**, **Camera Pilot** 이었습니다.

- **WEEK13 기간**: 2025-11-28 ~ 12-04 (7일), 팀 4명, 본인 커밋 106건
- **엔진(Mundi) 개발**: 2025-09-02 ~ 12-04. 매 주차 3~4명 팀이 랜덤으로 새로 짜이고, 팀원 중 한 명의 전 주차 코드베이스를 골라 그 위에 새 feature 를 얹는 방식
- **엔진은 WEEK13 팀 4명이 만든 것이 아닙니다.** 이 코드베이스에는 여러 주차 팀의 작업이 누적되어 있고, 본인 코드는 전체의 12.2% 입니다. 이 계보에서 본인이 참여한 주차는 WEEK07 · WEEK11 · WEEK13 셋입니다
- **기술 스택**: C++20 · DirectX 11 · NVIDIA PhysX · NvCloth · Lua + sol2 · Dear ImGui · DirectXTK / DirectXTex · Autodesk FBX SDK · Python 코드 생성기 · Visual Studio (MSVC)

## 핵심 작업

문서는 구현 전·중에 쓴 작업 문서 그대로입니다. 면접용으로 다시 쓴 것이 아닙니다. 예외는 [PhysicsAsyncProfiling.md](Mundi/Docs/PhysicsAsyncProfiling.md) 로, WEEK13 종료 후 비동기 전환의 효과를 측정해 쓴 문서입니다.

### 물리 — PhysX 통합과 멀티스레딩

- [**PhysicsMultithread.md**](Mundi/Docs/PhysicsMultithread.md) — PhysX 물리 시뮬레이션에 멀티스레드 안전성을 넣는 계획. UE Chaos 를 참고한 scene read/write lock, `Simulate`/`FetchResults`·actor 추가 제거 경로의 잠금, 이벤트 콜백 안전성, 시뮬레이션 중 수정 요청의 지연 처리
- [**PhysicsAsyncSimulation.md**](Mundi/Docs/PhysicsAsyncSimulation.md) — 비동기 물리 시뮬레이션이 **어떻게 구현돼 있고 어떤 순서로 동작하는지**. `simulate()`/`fetchResults()` 사이에 Actor Tick 을 끼워 넣는 프레임 구조, fixed timestep 과 렌더 보간, 겹치는 동안의 읽기(직전 스텝 값)·쓰기(지연 큐)·속도(캐시) 처리. 코드베이스를 모르는 사람이 읽도록 쓴 문서이고 코드 링크가 달려 있습니다
- [**PhysicsAsyncProfiling.md**](Mundi/Docs/PhysicsAsyncProfiling.md) — 위 비동기 전환의 효과 측정. 동기/비동기를 토글해 프레임 시간을 비교했고, 8000 dynamic body 에서 **17.6% 감소**. 이득이 `min(물리 시간, Actor Tick 시간)` 상한 아래를 따라가는 것까지 확인했습니다. 측정 하네스와 분석 스크립트는 `Mundi/Source/Runtime/Engine/Physics/PhysBench.*` · `Tools/PhysBench/` 에 있습니다
- [PLAN_TeamA_PhysX_Integration.md](Mundi/Docs/PLAN_TeamA_PhysX_Integration.md) — PhysX 를 엔진에 붙이는 통합 계획
- [PLAN_FBodyInstance.md](Mundi/Docs/PLAN_FBodyInstance.md) · [PLAN_PhysicsEventCallback.md](Mundi/Docs/PLAN_PhysicsEventCallback.md) — UE 의 `FBodyInstance` · `PxSimulationEventCallback` 대응 설계
- [UnrealEngine_Physics_Architecture.md](Mundi/Docs/UnrealEngine_Physics_Architecture.md) · [UnrealEngine_Ragdoll_Implementation.md](Mundi/Docs/UnrealEngine_Ragdoll_Implementation.md) — 참고한 UE 구조 분석
- [PhysicsAssetEditor_SimulateImplementation.md](Mundi/Docs/PhysicsAssetEditor_SimulateImplementation.md) — Physics Asset Editor 의 시뮬레이션 실행

코드는 `Mundi/Source/Runtime/Engine/Physics/` 에 있고, 이 디렉터리 본인 지분은 **3,640 / 7,257줄 (50.2%)** 입니다. WEEK13 종료 시점 기준이며, 이후 추가한 프로파일링 하네스(`PhysBench.*`)는 빠져 있습니다.

### [Depth of Field](Mundi/Docs/DepthOfField_Analysis.md)

UE 의 Focus Distance · F-Stop · Focal Length 파라미터로 CoC 를 계산하는 포스트프로세스입니다. Phase 1 은 full-resolution 단일 패스 DiskBlur 였고, Phase 2 에서 half-resolution 다운샘플 → separable blur → 업샘플 합성의 다중 패스로 바꿨습니다. 렌더패스 · 셰이더 7개 · 카메라 모디파이어까지 **924 / 924줄 (100%)** 본인 단독 작성입니다. 설계는 [DepthOfField_RenderingPass.md](Mundi/Docs/DepthOfField_RenderingPass.md) · [DepthOfField_Phase2_Implementation.md](Mundi/Docs/DepthOfField_Phase2_Implementation.md) 에 있습니다.

### [Camera Pilot](Mundi/Docs/CameraPilotMode_Implementation.md)

씬에 배치된 `UCameraComponent` 보유 액터를 뷰포트 드롭다운에서 고르면, 그 카메라 시점으로 직접 이동·회전하며 구도를 잡는 에디터 모드입니다. PIE 없이 DoF 효과를 카메라 시점에서 확인하려고 만들었습니다. 기존 뷰포트·툴바 코드 위에 얹은 작업입니다.

### 그 외 — WEEK07 에 만들어 남아 있는 것

이 코드베이스의 조명 시스템(Directional / Point / Spot / Ambient, 감쇠 2모드, 색온도)과 텍스처 DDS 베이킹 · 셰이더 핫 리로드는 WEEK07 에 본인이 만든 것입니다. 이후 주차에 팀원의 그림자 매핑 · 타일 라이트 컬링이 같은 파일에 쌓여 지분이 희석됐습니다. 상세는 같은 계보의 다음 저장소 문서에 정리했습니다 — [IntoTheDeadline](https://github.com/nansu0425/IntoTheDeadline).

## 기여 통계

파일별 `git blame` 지분과 산정 방법, 팀원이 만든 것, 커밋 이력 주의사항은 [Docs/Contribution.md](Docs/Contribution.md) 에 있습니다.

## 이 저장소에 대해

**소스 코드 공개용 저장소입니다. 빌드되지 않습니다** — 원본 팀 저장소에서 코드만 추출했고, 재배포할 수 없거나 재배포에 문제 소지가 있는 아래 항목을 제외했습니다.

| 제외 | 이유 |
|---|---|
| 캐릭터 · 애니메이션 FBX (13개, 172 MB) | Adobe Mixamo 등. 프로젝트 내 사용은 허용되나 에셋 재배포 금지 |
| 모델 · 텍스처 (`.obj` `.mtl` `.png` `.jpg` `.dds` `.tif`, 102개) | 출처와 라이선스를 확인할 수 없음 |
| 오디오 (`.wav`, 8개) | 출처 불명 |
| 맑은 고딕 폰트 (`.ttf`, 3개) | © Microsoft, Windows 번들 폰트 |
| 에디터 아이콘 (`.png` `.dds` `.ico`, 94개) | 출처와 라이선스를 확인할 수 없음 |
| 씬 · 프리팹 · 파티클 데이터 (`.scene` `.prefab` `.particle`, 43개) | 위 에셋을 참조하는 데이터. 에셋 없이는 로드되지 않음 |
| 서드파티 라이브러리 (PhysX · NvCloth · DirectXTK / DirectXTex · FBX SDK · Lua · sol2 · nlohmann · Dear ImGui) | 각 upstream 에서 받아야 함. FBX SDK 는 재배포 금지 |

제외 규칙 전체는 [`.gitignore`](.gitignore) 에 이유와 함께 적혀 있습니다. 셰이더(`.hlsl`) · Lua 스크립트 · 빌드 스크립트 · 프로젝트 파일은 소스로 보고 남겼습니다.

- **원본 저장소의 커밋 이력과 기여자 27명의 정보를 그대로 보존했습니다.** 작성자·날짜·커밋 메시지가 원본과 같아 기여 통계를 여기서 `git blame` 으로 검증할 수 있습니다. 다만 에셋·서드파티를 이력에서도 걸러냈기 때문에 커밋 해시는 원본과 다릅니다. 자세한 것은 [Contribution.md](Docs/Contribution.md#커밋-이력에-대한-주의사항) 를 보세요.
- `Mundi/Docs/` 에는 **본인이 100% 작성한 문서 13편만** 남겼습니다. 팀원 작성 문서는 옮기지 않았습니다.
- `Mundi/Docs/Mundi_CoordinateSystem.md` 는 원본 저장소의 최상위 README(엔진 좌표계 규약, 팀원 작성)를 옮긴 것입니다.
- KRAFTON 정글 게임테크랩 2기 교육과정 산출물이며, 포트폴리오 목적으로 코드만 공개합니다. 별도 라이선스를 두지 않았습니다 — 공동 저작물이므로 코드 재사용을 원하시면 문의해 주세요.
