# 기여 통계

이 저장소는 여러 주차 팀의 작업이 누적된 공동 작업물이고, 본인(nansu0425) 코드는 전체의 일부입니다. 무엇이 본인 작업이고 무엇이 아닌지를 정량적으로 밝힙니다.

## 산정 방법

- `git blame -w --line-porcelain` 으로 각 줄을 마지막으로 수정한 author 를 집계했습니다. `-w` 는 공백만 바뀐 줄을 원저자에게 남깁니다.
- 본인 식별은 author 이메일 `nansu0425@gmail.com` 기준입니다 (계정명 `nansu0425` / `NanSu` 두 가지로 커밋됨).
- 표의 수치는 **현재 코드에 남아 있는 본인 작성 줄 수 / 파일 전체 줄 수** 입니다. 커밋 수를 세는 방식과 달리, 본인이 작성했더라도 이후 팀원이 고쳐 쓴 줄은 본인 지분에서 빠집니다.
- 대상은 **이 저장소가 원본에서 가져온 추적 파일 전체** 입니다 (빌드 스크립트·문서 포함). 제외한 것(서드파티 라이브러리·에셋)은 분모에도 들어가지 않습니다. 제외 목록은 [`.gitignore`](../.gitignore) 에 있습니다. 이 저장소에서 새로 쓴 문서(README, 이 문서)는 분모에 넣지 않았습니다.
- 산정 기준 커밋: 이 저장소의 `bee3f6bd` (원본 저장소의 `d2a4ee74`, 2025-12-04, main HEAD).

재현하려면 파일마다 이렇게 셉니다.

```sh
git blame -w --line-porcelain <file> | grep '^author ' | sort | uniq -c
```

## 전체 지분

| 구분 | 본인 줄 / 전체 줄 | 비율 |
|---|---:|---:|
| 전체 | 16,859 / 137,788 | 12.2% |
| 설계 문서 (`*.md`) | 6,194 / 6,736 | 92.0% |
| HLSL 셰이더 | 1,409 / 4,263 | 33.1% |
| C++ (`*.cpp` `*.h`) | 8,952 / 114,717 | 7.8% |
| Lua | 138 / 2,806 | 4.9% |

문서 비율이 높은 것은 이 저장소가 **본인이 쓴 문서만** 옮겼기 때문입니다 (아래 [문서](#문서) 참고). 원본 `Mundi/Docs/` 전체로 보면 10,670줄 중 본인 7,071줄(66.3%)입니다.

커밋 수 기준으로는 팀 전체 2,291건 중 본인 243건입니다.

## 이 저장소의 계보

이 코드베이스는 KRAFTON 정글 게임테크랩 2기 교육과정에서 **매 주차 3~4명 팀이 새로 짜이고, 팀원 중 한 명의 전 주차 코드베이스를 골라 그 위에 새 feature 를 얹는** 방식으로 누적된 것입니다. 이 저장소의 이력은 2025-09-26 부터 2025-12-04(WEEK13 종료) 까지이고, 그 구간에서 본인이 참여한 주차는 셋입니다.

| 주차 | 기간 | 본인 커밋 | 본인 작업 |
|---|---|---:|---|
| WEEK07 | 2025-10-16 ~ 10-23 | 133건 | 조명 시스템, 텍스처 DDS 베이킹·셰이더 핫 리로드 |
| WEEK11 | 2025-11-13 ~ 11-14 | 4건 | 애니메이션 프레임워크 초기 골격, Lua 라이브러리 버전 교체 |
| WEEK13 | 2025-11-28 ~ 12-04 | 106건 | PhysX 통합 · 물리 멀티스레딩, Depth of Field, Camera Pilot |

나머지 주차는 다른 팀들의 작업이며, 본인이 참여하지 않았습니다.

## WEEK13 — PhysX 통합과 물리 멀티스레딩

핵심 문서: [`Mundi/Docs/PhysicsMultithread.md`](../Mundi/Docs/PhysicsMultithread.md)

`Mundi/Source/Runtime/Engine/Physics/` 디렉터리 합계 **3,640 / 7,257줄 (50.2%)**. 이 디렉터리는 본인과 팀원 여러 명이 나눠 작업했습니다.

| 파일 | 본인 줄 / 전체 줄 |
|---|---:|
| `PhysScene.cpp` | 882 / 1,151 (76.6%) |
| `BodyInstance.cpp` | 557 / 793 (70.2%) |
| `PhysicsEventCallback.cpp` | 243 / 253 (96.0%) |
| `PhysicsCore.cpp` | 232 / 277 (83.8%) |
| `PhysicsTypes.h` | 223 / 223 (100%) |
| `BodyInstance.h` | 218 / 251 (86.9%) |
| `PhysSceneImpl.h` | 206 / 223 (92.4%) |
| `PhysicsStats.h` | 187 / 187 (100%) |
| `HitResult.h` | 161 / 161 (100%) |
| `PhysScene.h` | 144 / 185 (77.8%) |
| `BodySetup.cpp` | 110 / 291 (37.8%) |
| `PhysicsEventCallback.h` | 99 / 99 (100%) |
| `PhysicsSceneLock.h` | 95 / 95 (100%) |
| `BodySetup.h` | 76 / 116 (65.5%) |
| `PhysicsCore.h` | 69 / 73 (94.5%) |
| `BodyInstanceImpl.h` | 67 / 69 (97.1%) |
| `ConstraintInstance.cpp` | 53 / 415 (12.8%) |
| `BodyInstanceImpl.cpp` | 18 / 18 (100%) |

물리 테스트용 액터 4종(`PhysBoxActor` · `PhysSphereActor` · `PhysCapsuleActor` · `PhysGroundActor`, cpp)은 355 / 355줄 (100%) 입니다.

기존 시스템에 연결한 통합 지점 (파일 자체는 팀원 작성, 본인은 일부 추가):

| 파일 | 본인 줄 / 전체 줄 |
|---|---:|
| `.../Engine/Components/SkeletalMeshComponent.cpp` (래그돌 연동) | 403 / 928 (43.4%) |
| `.../Engine/Components/PrimitiveComponent.h` | 111 / 201 (55.2%) |
| `.../Engine/Components/PrimitiveComponent.cpp` | 86 / 161 (53.4%) |
| `.../Engine/Components/ShapeComponent.cpp` | 82 / 213 (38.5%) |
| `.../Slate/Windows/SPhysicsAssetEditorWindow.cpp` | 294 / 4,784 (6.1%) |
| `Mundi/Data/Scripts/OverlapTest.lua` | 85 / 85 (100%) |

`PhysicsAsset.cpp`(0 / 538) · `ConstraintInstance.h`(0 / 263) · `Shape/*`(0 / 379) · `AggregateGeom`(0 / 214) 과 Physics Asset Editor 창 본체는 팀원 작업입니다. NvCloth 옷감(`Cloth*`, 0 / 1,377) 과 차량(`VehicleMovementComponent`, 0 / 1,231) 도 팀원 작업입니다.

## WEEK13 — Depth of Field

설계 문서: [`DepthOfField_Analysis.md`](../Mundi/Docs/DepthOfField_Analysis.md) · [`DepthOfField_RenderingPass.md`](../Mundi/Docs/DepthOfField_RenderingPass.md) · [`DepthOfField_Phase2_Implementation.md`](../Mundi/Docs/DepthOfField_Phase2_Implementation.md)

렌더패스 · 셰이더 · 카메라 모디파이어 전부 본인 단독 작성입니다 — 합계 **924 / 924줄 (100%)**.

| 파일 | 본인 줄 / 전체 줄 |
|---|---:|
| `.../Renderer/PostProcessing/DepthOfFieldPass.cpp` | 320 / 320 (100%) |
| `Mundi/Shaders/PostProcess/DoF_Simple_PS.hlsl` | 141 / 141 (100%) |
| `Mundi/Shaders/PostProcess/DoF_Common.hlsli` | 85 / 85 (100%) |
| `Mundi/Shaders/PostProcess/DoF_Downsample_PS.hlsl` | 82 / 82 (100%) |
| `Mundi/Shaders/PostProcess/DoF_HorizontalBlur_PS.hlsl` | 63 / 63 (100%) |
| `Mundi/Shaders/PostProcess/DoF_VerticalBlur_PS.hlsl` | 63 / 63 (100%) |
| `Mundi/Shaders/PostProcess/DoF_Upsample_PS.hlsl` | 53 / 53 (100%) |
| `.../Renderer/PostProcessing/DepthOfFieldPass.h` | 40 / 40 (100%) |
| `Mundi/Shaders/PostProcess/DoF_FullScreen_VS.hlsl` | 33 / 33 (100%) |
| `.../Engine/GameFramework/Camera/CamMod_DoF.cpp` | 23 / 23 (100%) |
| `.../Engine/GameFramework/Camera/CamMod_DoF.h` | 21 / 21 (100%) |

## WEEK13 — Camera Pilot

설계 문서: [`CameraPilotMode_Implementation.md`](../Mundi/Docs/CameraPilotMode_Implementation.md)

에디터에서 액터를 카메라로 조종하는 모드입니다. 전부 기존 뷰포트·툴바 코드에 얹은 작업이라 파일 단위로는 팀원 코드와 섞여 있습니다.

| 파일 | 본인 줄 / 전체 줄 |
|---|---:|
| `.../Renderer/FViewportClient.cpp` | 330 / 761 (43.4%) |
| `.../Renderer/FViewportClient.h` | 29 / 118 (24.6%) |
| `.../Slate/Windows/SViewportWindow.cpp` | 208 / 2,487 (8.4%) |
| `.../Slate/SlateManager.cpp` | 16 / 1,062 (1.5%) |

`FViewportClient` 의 본인 지분에는 Camera Pilot 외의 뷰포트 작업도 섞여 있으므로, 위 수치를 Camera Pilot 하나의 크기로 읽으면 안 됩니다.

## WEEK07 — 조명 시스템 · 에셋 파이프라인

WEEK07 에 만든 조명·에셋 인프라가 이 코드베이스에 남아 있습니다. 이후 주차에 팀원의 그림자 매핑·타일 라이트 컬링 작업이 같은 파일에 쌓여 지분이 희석됐습니다.

| 파일 | 본인 줄 / 전체 줄 |
|---|---:|
| `.../Engine/Components/SpotLightComponent.cpp` | 377 / 806 (46.8%) |
| `Mundi/Shaders/Common/LightingCommon.hlsl` | 334 / 932 (35.8%) |
| `Mundi/Shaders/Materials/UberLit.hlsl` | 308 / 647 (47.6%) |
| `.../AssetManagement/TextureConverter.cpp` | 240 / 283 (84.8%) |
| `.../Renderer/Shader.cpp` (핫 리로드) | 212 / 655 (32.4%) |
| `Mundi/Shaders/Effects/Decal.hlsl` | 110 / 209 (52.6%) |
| `.../Engine/Components/PointLightComponent.cpp` | 96 / 221 (43.4%) |
| `.../AssetManagement/TextureConverter.h` | 87 / 87 (100%) |
| `.../Core/Misc/PathUtils.h` (인코딩 유틸) | 87 / 177 (49.2%) |
| `.../AssetManagement/Texture.cpp` | 73 / 167 (43.7%) |
| `.../Engine/Components/LightComponent.cpp` | 72 / 127 (56.7%) |
| `.../Renderer/Shader.h` | 15 / 146 (10.3%) |

이 두 영역의 상세는 같은 계보의 다음 저장소 문서에 정리했습니다 — [IntoTheDeadline / Feature_Lighting.md · Feature_AssetPipeline.md](https://github.com/nansu0425/IntoTheDeadline).

## 팀원이 만든 것

혼동을 막기 위해 **본인이 만들지 않은 주요 기능** 을 밝힙니다. 수치는 같은 방법으로 측정한 본인 지분입니다.

| 기능 | 경로 | 본인 지분 |
|---|---|---:|
| 파티클 시스템 | `.../Engine/Particles/` | 5 / 7,550 (0.1%) |
| 파티클 에디터 | `.../Slate/Windows/SParticleEditorWindow.cpp` | 0 / 3,149 (0%) |
| 스켈레탈 애니메이션 | `.../Engine/Animation/` | 0 / 2,847 (0%) |
| 리플렉션 · Lua 바인딩 | `.../Engine/Scripting/` | 0 / 2,836 (0%) |
| 코드 생성기 (Python) | `Mundi/BuildTools/CodeGenerator/` | 0 / 3,478 (0%) |
| BVH · 공간 자료구조 | `.../Engine/Spatial/` | 0 / 2,250 (0%) |
| 충돌 질의 | `.../Engine/Collision/` | 6 / 3,626 (0.2%) |
| 그림자 매핑 (PSM / LiSPSM 워핑 포함) | `SpotLightComponent.cpp` L300~470, `Shaders/Shadows/` | 0 / 278 (0%) |
| 타일 라이트 컬링 | `.../Renderer/TileLightCuller.cpp`, `LightManager.cpp` | 8 / 1,242 (0.6%) |
| 머티리얼 | `.../Renderer/Material.cpp` | 0 / 531 (0%) |
| 애니메이션 · 스켈레탈 뷰어 창 | `.../Slate/Windows/SViewerWindow.cpp` 외 3개 | 5 / 5,607 (0.1%) |

> 이 교육과정은 매 주차 팀과 코드베이스가 바뀌었습니다. 본인이 어느 주차에 만든 기능이라도, 그 주차 코드베이스가 이 저장소의 조상이 아니면 여기 들어있지 않습니다.

## 문서

`Mundi/Docs/` 에는 **본인이 100% 작성한 문서 11편만** 남겼습니다. 팀원이 작성한 문서 7편과, 본인이 작성했지만 기술 결과물이 아닌 교육과정 발제 문서·팀 작업 분배표 2편은 옮기지 않았습니다 (목록은 [`.gitignore`](../.gitignore)).

| 문서 | 줄 수 |
|---|---:|
| [`PLAN_PhysicsEventCallback.md`](../Mundi/Docs/PLAN_PhysicsEventCallback.md) | 1,012 |
| [`DepthOfField_Phase2_Implementation.md`](../Mundi/Docs/DepthOfField_Phase2_Implementation.md) | 845 |
| [`PLAN_FBodyInstance.md`](../Mundi/Docs/PLAN_FBodyInstance.md) | 807 |
| [`UnrealEngine_Ragdoll_Implementation.md`](../Mundi/Docs/UnrealEngine_Ragdoll_Implementation.md) | 638 |
| [`DepthOfField_RenderingPass.md`](../Mundi/Docs/DepthOfField_RenderingPass.md) | 632 |
| [`UnrealEngine_Physics_Architecture.md`](../Mundi/Docs/UnrealEngine_Physics_Architecture.md) | 484 |
| [`CameraPilotMode_Implementation.md`](../Mundi/Docs/CameraPilotMode_Implementation.md) | 479 |
| [`DepthOfField_Analysis.md`](../Mundi/Docs/DepthOfField_Analysis.md) | 420 |
| [`PhysicsMultithread.md`](../Mundi/Docs/PhysicsMultithread.md) | 338 |
| [`PLAN_TeamA_PhysX_Integration.md`](../Mundi/Docs/PLAN_TeamA_PhysX_Integration.md) | 270 |
| [`PhysicsAssetEditor_SimulateImplementation.md`](../Mundi/Docs/PhysicsAssetEditor_SimulateImplementation.md) | 269 |

전부 구현 전·중에 쓴 작업 문서이고, 면접관용으로 다시 정리한 것이 아닙니다. 당시 작업 기록 그대로입니다.

[`Mundi/Docs/Mundi_CoordinateSystem.md`](../Mundi/Docs/Mundi_CoordinateSystem.md) 는 원본 저장소의 최상위 README(엔진 좌표계 규약, 팀원 작성)를 옮긴 것입니다.

## 커밋 이력에 대한 주의사항

- **원본 저장소의 커밋 이력을 그대로 보존했습니다.** 작성자·날짜·커밋 메시지가 원본과 같고, 위 지분 수치는 이 저장소에서 `git blame` 으로 그대로 재현됩니다.
- 에셋·서드파티는 워킹 트리뿐 아니라 **이력에서도** 지웠습니다. 그래서 커밋 해시가 원본과 다르고, 에셋·서드파티만 건드린 커밋과 걸러낸 뒤 한쪽 부모와 같아진 병합 커밋 79개가 사라져 원본 2,291개 중 2,212개가 남았습니다 (본인 커밋은 243건 중 240건). 위의 커밋 수 2,291 / 243 은 원본 기준입니다.
- 산정 기준으로 삼은 원본 main HEAD `d2a4ee74` 도 그렇게 접힌 병합 커밋이라 이 저장소에는 없습니다. 같은 트리를 갖는 커밋은 `bee3f6bd` 입니다.
- 원본 저장소는 2025-09-26 에 당시 팀이 새로 만들면서 그때까지의 코드베이스를 임포트했으므로, 엔진 개발 시작(2025-09-02)부터의 이력은 원본에도 없습니다.
- 원본의 기여자는 게임테크랩 여러 주차 팀의 누적이며 27명입니다.
