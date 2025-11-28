#include "pch.h"
#include "SParticleEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/ParticleEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "PlatformProcess.h"
#include "PathUtils.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleSystemComponent.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Modules/ParticleModuleLifetime.h"
#include "Modules/ParticleModuleSize.h"
#include "Modules/ParticleModuleVelocity.h"
#include "Modules/ParticleModuleColor.h"
#include "Modules/ParticleModuleTypeDataSprite.h"
#include "Modules/ParticleModuleTypeDataMesh.h"
#include "Modules/ParticleModuleTypeDataRibbon.h"
#include "Modules/ParticleModuleTypeDataBeam.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleSpawn.h"
#include "Modules/ParticleModuleAcceleration.h"
#include "Modules/ParticleModuleLocation.h"
#include "Modules/ParticleModuleRotation.h"
#include "Modules/ParticleModuleMeshRotation.h"
#include "Modules/ParticleModuleRotationRate.h"
#include "Modules/ParticleModuleSizeScaleBySpeed.h"
#include "Modules/ParticleModuleCollision.h"
#include "Modules/ParticleModuleEventGenerator.h"
#include "Modules/ParticleModuleEventReceiver.h"
#include "Modules/ParticleModuleEventReceiverKill.h"
#include "Modules/ParticleModuleEventReceiverSpawn.h"
#include "Modules/ParticleModuleTrailSource.h"
#include "Material.h"
#include "StaticMesh.h"
#include "ResourceManager.h"
#include "Distribution.h"
#include "Shader.h"
#include "Widgets/PropertyRenderer.h"
#include "ParticleStats.h"
#include "ParticleEmitterInstance.h"
#include "RenderSettings.h"

// 모듈 드래그 앤 드롭 페이로드 구조체
struct FModuleDragPayload
{
	int32 EmitterIndex;
	UParticleModule* Module;
};

// static 변수 정의
bool SParticleEditorWindow::bIsAnyParticleEditorFocused = false;

SParticleEditorWindow::SParticleEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
	LeftPanelRatio = 0.35f;  // 뷰포트 + 디테일
	RightPanelRatio = 0.60f;  // 이미터 패널
	bHasBottomPanel = true;

	// 커브 에디터 위젯 생성
	CurveEditorWidget = NewObject<SCurveEditorWidget>();
	CurveEditorWidget->Initialize();
}

SParticleEditorWindow::~SParticleEditorWindow()
{
	// 커브 에디터 위젯 정리
	if (CurveEditorWidget)
	{
		DeleteObject(CurveEditorWidget);
		CurveEditorWidget = nullptr;
	}

	// 포커스 플래그 리셋 (윈도우 파괴 시)
	bIsAnyParticleEditorFocused = false;

	// 툴바 아이콘 정리
	if (IconSave)
	{
		DeleteObject(IconSave);
		IconSave = nullptr;
	}
	if (IconSaveAs)
	{
		DeleteObject(IconSaveAs);
		IconSaveAs = nullptr;
	}
	if (IconLoad)
	{
		DeleteObject(IconLoad);
		IconLoad = nullptr;
	}
	if (IconRestart)
	{
		DeleteObject(IconRestart);
		IconRestart = nullptr;
	}
	if (IconBounds)
	{
		DeleteObject(IconBounds);
		IconBounds = nullptr;
	}
	if (IconOriginAxis)
	{
		DeleteObject(IconOriginAxis);
		IconOriginAxis = nullptr;
	}
	if (IconBackgroundColor)
	{
		DeleteObject(IconBackgroundColor);
		IconBackgroundColor = nullptr;
	}
	if (IconLODFirst)
	{
		DeleteObject(IconLODFirst);
		IconLODFirst = nullptr;
	}
	if (IconLODPrev)
	{
		DeleteObject(IconLODPrev);
		IconLODPrev = nullptr;
	}
	if (IconLODInsertBefore)
	{
		DeleteObject(IconLODInsertBefore);
		IconLODInsertBefore = nullptr;
	}
	if (IconLODInsertAfter)
	{
		DeleteObject(IconLODInsertAfter);
		IconLODInsertAfter = nullptr;
	}
	if (IconLODDelete)
	{
		DeleteObject(IconLODDelete);
		IconLODDelete = nullptr;
	}
	if (IconLODNext)
	{
		DeleteObject(IconLODNext);
		IconLODNext = nullptr;
	}
	if (IconLODLast)
	{
		DeleteObject(IconLODLast);
		IconLODLast = nullptr;
	}

	// 탭 정리 (OriginAxisLineComponent는 탭별 State가 소유하므로 따로 정리 불필요)
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		ParticleEditorBootstrap::DestroyViewerState(State);
	}
	Tabs.Empty();
	ActiveState = nullptr;
}

// 타입 데이터 삭제 시 스프라이트로 복원 (머티리얼 포함, 모든 LOD에 동기화)
static void RestoreToSpriteTypeData(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	UParticleEmitter* Emitter = State->EditingTemplate ?
		(State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < State->EditingTemplate->Emitters.Num() ?
			State->EditingTemplate->Emitters[State->SelectedEmitterIndex] : nullptr) : nullptr;

	auto ApplyToLOD = [](UParticleLODLevel* TargetLOD) {
		if (TargetLOD->TypeDataModule)
		{
			TargetLOD->Modules.Remove(TargetLOD->TypeDataModule);
			DeleteObject(TargetLOD->TypeDataModule);
			TargetLOD->TypeDataModule = nullptr;
		}

		UParticleModuleTypeDataSprite* SpriteTypeData = NewObject<UParticleModuleTypeDataSprite>();
		TargetLOD->TypeDataModule = SpriteTypeData;
		TargetLOD->Modules.Add(SpriteTypeData);

		if (TargetLOD->RequiredModule)
		{
			UMaterialInterface* OldMaterial = TargetLOD->RequiredModule->Material;
			if (OldMaterial && OldMaterial->GetFilePath().empty())
			{
				DeleteObject(OldMaterial);
			}

			UMaterial* SpriteMaterial = NewObject<UMaterial>();
			UShader* SpriteShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleSprite.hlsl");
			SpriteMaterial->SetShader(SpriteShader);

			FMaterialInfo MatInfo;
			MatInfo.DiffuseTextureFileName = GDataDir + "/Textures/Particles/OrientParticle.png";
			SpriteMaterial->SetMaterialInfo(MatInfo);
			SpriteMaterial->ResolveTextures();

			TargetLOD->RequiredModule->Material = SpriteMaterial;
			TargetLOD->RequiredModule->bOwnsMaterial = true;
		}

		TargetLOD->CacheModuleInfo();
	};

	if (Emitter)
	{
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (LODLevel) ApplyToLOD(LODLevel);
		}
	}
	else
	{
		ApplyToLOD(LOD);
	}
}

void SParticleEditorWindow::OnRender()
{
	// 매 프레임 시작 시 포커스 플래그 리셋 (윈도우가 닫혀도 리셋되도록)
	bIsAnyParticleEditorFocused = false;

	// 윈도우가 닫혔으면 정리 요청
	if (!bIsOpen)
	{
		USlateManager::GetInstance().RequestCloseDetachedWindow(this);
		return;
	}

	// 윈도우 플래그
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	// 초기 배치 (첫 프레임)
	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	// 포커스 요청
	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	// 윈도우 타이틀 생성
	char UniqueTitle[256];
	FString Title = GetWindowTitle();
	sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

	// 첫 프레임에 아이콘 로드
	if (!IconSave && Device)
	{
		LoadToolbarIcons();
	}

	if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
	{
		// hover/focus 상태 캡처
		bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 다른 위젯에서 참조할 수 있도록 static 변수에도 저장
		bIsAnyParticleEditorFocused = bIsWindowFocused;

		// 탭바 및 툴바 렌더링
		RenderTabsAndToolbar(EViewerType::Particle);

		// 마지막 탭을 닫은 경우
		if (!bIsOpen)
		{
			USlateManager::GetInstance().RequestCloseDetachedWindow(this);
			ImGui::End();
			return;
		}

		// 윈도우 rect 업데이트
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x; 
		Rect.Top = pos.y; 
		Rect.Right = pos.x + size.x; 
		Rect.Bottom = pos.y + size.y;
		Rect.UpdateMinMax();

		// 툴바 렌더링
		RenderToolbar();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		// 좌우 패널 너비 계산
		float leftWidth = totalWidth * LeftPanelRatio;
		leftWidth = ImClamp(leftWidth, 200.f, totalWidth - 300.f);

		// 좌측 열 Render (뷰포트 + 디테일)
		ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, 0), false);
		{
			RenderLeftViewportArea(totalHeight);
			RenderLeftDetailsArea();
		}
		ImGui::EndChild();

		// 수직 스플리터 (좌우 열 간)
		ImGui::SameLine();
		ImGui::Button("##VSplitter", ImVec2(4.f, -1));
		if (ImGui::IsItemActive())
		{
			float delta = ImGui::GetIO().MouseDelta.x;
			LeftPanelRatio += delta / totalWidth;
			LeftPanelRatio = ImClamp(LeftPanelRatio, 0.15f, 0.7f);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		ImGui::SameLine();

		// 우측 열 Render (이미터 패널 + 커브 에디터)
		ImGui::BeginChild("RightColumn", ImVec2(0, 0), false);
		{
			RenderRightEmitterArea(totalHeight);
			RenderRightCurveArea();
		}
		ImGui::EndChild();

		// Delete 키로 선택된 이미터/모듈 삭제
		ParticleEditorState* State = GetActiveParticleState();
		if (bIsWindowFocused && State && ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			UParticleSystem* System = State->EditingTemplate;
			if (System)
			{
				// 모듈이 선택된 경우
				if (State->SelectedModule && State->SelectedEmitterIndex >= 0)
				{
					UParticleEmitter* Emitter = System->Emitters[State->SelectedEmitterIndex];
					if (Emitter)
					{
						UParticleLODLevel* LOD = Emitter->GetLODLevel(State->CurrentLODLevel);
						if (LOD)
						{
							UParticleModule* Module = State->SelectedModule;

							// 삭제 불가 모듈 체크 (Required, Spawn, 스프라이트 타입데이터)
							// LOD 0에서만 모듈 삭제 가능
							bool bCanDelete = (State->CurrentLODLevel == 0);
							if (Module == LOD->RequiredModule || Module == LOD->SpawnModule)
							{
								bCanDelete = false;
							}
							if (Cast<UParticleModuleTypeDataSprite>(Module))
							{
								bCanDelete = false;
							}

							if (bCanDelete)
							{
								// TypeDataModule인 경우 (메시/리본/빔 삭제 시 스프라이트로 복원)
								if (Module == LOD->TypeDataModule)
								{
									RestoreToSpriteTypeData(LOD, State);
								}
								else
								{
									// 모든 LOD에서 동일 인덱스 모듈 삭제
									int32 ModuleIndex = LOD->Modules.Find(Module);
									if (ModuleIndex >= 0)
									{
										for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
										{
											if (LODLevel && ModuleIndex < LODLevel->Modules.Num())
											{
												UParticleModule* ModuleToRemove = LODLevel->Modules[ModuleIndex];
												LODLevel->Modules.RemoveAt(ModuleIndex);
												if (ModuleToRemove)
												{
													DeleteObject(ModuleToRemove);
												}
												LODLevel->CacheModuleInfo();
											}
										}
									}
								}

								State->SelectedModule = nullptr;
								State->SelectedModuleIndex = -1;
								State->bIsDirty = true;
								if (State->PreviewComponent)
								{
									State->PreviewComponent->RefreshEmitterInstances();
								}
							}
						}
					}
				}
				// 이미터만 선택된 경우 (모듈 선택 없음)
				else if (State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < System->Emitters.Num())
				{
					System->Emitters.RemoveAt(State->SelectedEmitterIndex);
					State->SelectedEmitterIndex = -1;
					State->SelectedModuleIndex = -1;
					State->SelectedModule = nullptr;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}

		// 좌/우 방향키로 이미터 순서 변경
		if (bIsWindowFocused && State && State->SelectedEmitterIndex >= 0)
		{
			UParticleSystem* System = State->EditingTemplate;
			if (System)
			{
				int32 EmitterIndex = State->SelectedEmitterIndex;
				int32 EmitterCount = System->Emitters.Num();

				// 왼쪽 방향키: 이미터를 왼쪽으로 이동
				if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && EmitterIndex > 0)
				{
					System->Emitters.Swap(EmitterIndex, EmitterIndex - 1);
					State->SelectedEmitterIndex--;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
				// 오른쪽 방향키: 이미터를 오른쪽으로 이동
				else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && EmitterIndex < EmitterCount - 1)
				{
					System->Emitters.Swap(EmitterIndex, EmitterIndex + 1);
					State->SelectedEmitterIndex++;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}

		// 타입데이터 중복 팝업
		if (bShowTypeDataExistsPopup)
		{
			ImGui::OpenPopup("TypeDataExistsPopup");
			bShowTypeDataExistsPopup = false;
		}

		if (ImGui::BeginPopupModal("TypeDataExistsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("타입데이터 모듈이 이미 존재합니다.\n먼저 제거해주세요.");
			ImGui::Separator();

			if (ImGui::Button("확인", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::End();
}

void SParticleEditorWindow::OnUpdate(float DeltaSeconds)
{
	SViewerWindow::OnUpdate(DeltaSeconds);

	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !State->PreviewComponent)
		return;

	// 시뮬레이션 속도 적용 (일시정지면 0, 아니면 SimulationSpeed)
	float EffectiveSpeed = State->bIsSimulating ? State->SimulationSpeed : 0.0f;
	State->PreviewComponent->SetSimulationSpeed(EffectiveSpeed);

	// 루프 체크: 모든 이미터가 완료되었으면 재시작
	if (bIsLooping && State->bIsSimulating)
	{
		bool bAllEmittersComplete = true;
		for (FParticleEmitterInstance* Instance : State->PreviewComponent->EmitterInstances)
		{
			if (Instance && Instance->bEmitterEnabled)
			{
				bAllEmittersComplete = false;
				break;
			}
		}

		if (bAllEmittersComplete && State->PreviewComponent->EmitterInstances.Num() > 0)
		{
			State->PreviewComponent->ResetParticles();
		}
	}
}

// AABB 기반 와이어프레임 박스 라인 생성 (12개 라인)
static void CreateBoundsBoxLines(ULineComponent* LineComp, const FVector& Min, const FVector& Max, const FVector4& Color)
{
	// 8개 꼭짓점
	FVector Corners[8] = {
		FVector(Min.X, Min.Y, Min.Z),  // 0: ---
		FVector(Max.X, Min.Y, Min.Z),  // 1: +--
		FVector(Max.X, Max.Y, Min.Z),  // 2: ++-
		FVector(Min.X, Max.Y, Min.Z),  // 3: -+-
		FVector(Min.X, Min.Y, Max.Z),  // 4: --+
		FVector(Max.X, Min.Y, Max.Z),  // 5: +-+
		FVector(Max.X, Max.Y, Max.Z),  // 6: +++
		FVector(Min.X, Max.Y, Max.Z),  // 7: -++
	};

	// 아래 면 (4개 라인)
	LineComp->AddLine(Corners[0], Corners[1], Color);
	LineComp->AddLine(Corners[1], Corners[2], Color);
	LineComp->AddLine(Corners[2], Corners[3], Color);
	LineComp->AddLine(Corners[3], Corners[0], Color);

	// 위 면 (4개 라인)
	LineComp->AddLine(Corners[4], Corners[5], Color);
	LineComp->AddLine(Corners[5], Corners[6], Color);
	LineComp->AddLine(Corners[6], Corners[7], Color);
	LineComp->AddLine(Corners[7], Corners[4], Color);

	// 수직 연결 (4개 라인)
	LineComp->AddLine(Corners[0], Corners[4], Color);
	LineComp->AddLine(Corners[1], Corners[5], Color);
	LineComp->AddLine(Corners[2], Corners[6], Color);
	LineComp->AddLine(Corners[3], Corners[7], Color);
}

// 구 와이어프레임 라인 생성 (3개 원: XY, XZ, YZ 평면)
static void CreateBoundsSphereLines(ULineComponent* LineComp, const FVector& Center, float Radius, const FVector4& Color)
{
	const int32 NumSegments = 32;
	const float PI_CONST = 3.14159265358979323846f;

	// XY 평면 원
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
		float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

		FVector P1 = Center + FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.0f);
		FVector P2 = Center + FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.0f);
		LineComp->AddLine(P1, P2, Color);
	}

	// XZ 평면 원
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
		float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

		FVector P1 = Center + FVector(cos(Angle1) * Radius, 0.0f, sin(Angle1) * Radius);
		FVector P2 = Center + FVector(cos(Angle2) * Radius, 0.0f, sin(Angle2) * Radius);
		LineComp->AddLine(P1, P2, Color);
	}

	// YZ 평면 원
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
		float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

		FVector P1 = Center + FVector(0.0f, cos(Angle1) * Radius, sin(Angle1) * Radius);
		FVector P2 = Center + FVector(0.0f, cos(Angle2) * Radius, sin(Angle2) * Radius);
		LineComp->AddLine(P1, P2, Color);
	}
}

// 현재 활성 파티클들의 Bounds 확장 (최대 크기 추적 - 언리얼 방식)
// 반환값: bounds가 확장되었으면 true
static bool ExpandParticleBounds(UParticleSystemComponent* PSC, FVector& InOutMin, FVector& InOutMax)
{
	bool bExpanded = false;

	for (FParticleEmitterInstance* Emitter : PSC->EmitterInstances)
	{
		if (!Emitter || Emitter->ActiveParticles == 0)
			continue;

		for (int32 i = 0; i < Emitter->ActiveParticles; ++i)
		{
			FBaseParticle* Particle = GetParticleAtIndex(Emitter, i);
			if (!Particle) continue;

			const FVector& Pos = Particle->Location;

			// Min 확장 (더 작은 값으로)
			if (Pos.X < InOutMin.X) { InOutMin.X = Pos.X; bExpanded = true; }
			if (Pos.Y < InOutMin.Y) { InOutMin.Y = Pos.Y; bExpanded = true; }
			if (Pos.Z < InOutMin.Z) { InOutMin.Z = Pos.Z; bExpanded = true; }

			// Max 확장 (더 큰 값으로)
			if (Pos.X > InOutMax.X) { InOutMax.X = Pos.X; bExpanded = true; }
			if (Pos.Y > InOutMax.Y) { InOutMax.Y = Pos.Y; bExpanded = true; }
			if (Pos.Z > InOutMax.Z) { InOutMax.Z = Pos.Z; bExpanded = true; }
		}
	}

	return bExpanded;
}

void SParticleEditorWindow::PreRenderViewportUpdate()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (State)
	{
		// 배경색 설정 (ViewportClient에 전달)
		if (State->Client)
		{
			State->Client->SetBackgroundColor(State->BackgroundColor);
		}

		// 원점축 LineComponent 가시성 제어 (각 탭이 자체 컴포넌트 소유)
		if (State->OriginAxisLineComponent)
		{
			State->OriginAxisLineComponent->SetLineVisible(State->bShowOriginAxis);
		}

		// Bounds 와이어프레임 표시 (최대 크기 추적 - 언리얼 방식)
		if (State->BoundsLineComponent)
		{
			if (State->bShowBounds && State->PreviewComponent)
			{
				// bounds 확장 체크 (확장되었을 때만 true 반환)
				bool bExpanded = ExpandParticleBounds(State->PreviewComponent, State->CachedBoundsMin, State->CachedBoundsMax);

				// bounds가 확장되었거나 재빌드 필요 시에만 라인 재생성
				if (bExpanded || State->bBoundsNeedRebuild)
				{
					State->BoundsLineComponent->ClearLines();

					// 유효한 bounds인지 확인
					if (State->CachedBoundsMin.X <= State->CachedBoundsMax.X)
					{
						// 중심과 반경 계산
						FVector Center = (State->CachedBoundsMin + State->CachedBoundsMax) * 0.5f;
						float Radius = FVector::Distance(Center, State->CachedBoundsMax);

						// 박스 (흰색)
						CreateBoundsBoxLines(State->BoundsLineComponent, State->CachedBoundsMin, State->CachedBoundsMax, FVector4(1.0f, 1.0f, 1.0f, 1.0f));

						// 구 (연한 파랑)
						CreateBoundsSphereLines(State->BoundsLineComponent, Center, Radius, FVector4(0.5f, 0.7f, 1.0f, 1.0f));
					}

					State->bBoundsNeedRebuild = false;
				}

				State->BoundsLineComponent->SetLineVisible(true);
			}
			else
			{
				State->BoundsLineComponent->SetLineVisible(false);
			}
		}
	}
}

void SParticleEditorWindow::LoadToolbarIcons()
{
	if (!Device) return;

	// 툴바 아이콘 로드
	IconSave = NewObject<UTexture>();
	IconSave->Load(GDataDir + "/Icon/Toolbar_Save.png", Device);

	IconSaveAs = NewObject<UTexture>();
	IconSaveAs->Load(GDataDir + "/Icon/Toolbar_SaveAs.png", Device);

	IconLoad = NewObject<UTexture>();
	IconLoad->Load(GDataDir + "/Icon/Toolbar_Load.png", Device);

	IconRestart = NewObject<UTexture>();
	IconRestart->Load(GDataDir + "/Icon/Toolbar_Restart.png", Device);

	IconBounds = NewObject<UTexture>();
	IconBounds->Load(GDataDir + "/Icon/Toolbar_Bounds.png", Device);

	IconOriginAxis = NewObject<UTexture>();
	IconOriginAxis->Load(GDataDir + "/Icon/Toolbar_OriginAxis.png", Device);

	IconBackgroundColor = NewObject<UTexture>();
	IconBackgroundColor->Load(GDataDir + "/Icon/Toolbar_BackgroundColor.png", Device);

	IconLODFirst = NewObject<UTexture>();
	IconLODFirst->Load(GDataDir + "/Icon/Toolbar_LODFirst.png", Device);

	IconLODPrev = NewObject<UTexture>();
	IconLODPrev->Load(GDataDir + "/Icon/Toolbar_LODPrev.png", Device);

	IconLODInsertBefore = NewObject<UTexture>();
	IconLODInsertBefore->Load(GDataDir + "/Icon/Toolbar_LODInsertBefore.png", Device);

	IconLODInsertAfter = NewObject<UTexture>();
	IconLODInsertAfter->Load(GDataDir + "/Icon/Toolbar_LODInsertAfter.png", Device);

	IconLODDelete = NewObject<UTexture>();
	IconLODDelete->Load(GDataDir + "/Icon/Toolbar_LODDelete.png", Device);

	IconLODNext = NewObject<UTexture>();
	IconLODNext->Load(GDataDir + "/Icon/Toolbar_LODNext.png", Device);

	IconLODLast = NewObject<UTexture>();
	IconLODLast->Load(GDataDir + "/Icon/Toolbar_LODLast.png", Device);

	IconCurve = NewObject<UTexture>();
	IconCurve->Load(GDataDir + "/Icon/Particle_Editor_Curve.png", Device);
}

ViewerState* SParticleEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
	return ParticleEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SParticleEditorWindow::DestroyViewerState(ViewerState*& State)
{
	// OriginAxisLineComponent는 각 탭의 PreviewActor가 소유하므로
	// World 삭제 시 자동으로 정리
	ParticleEditorBootstrap::DestroyViewerState(State);
}

void SParticleEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
	// 탭바만 렌더링 (뷰어 전환 버튼 제외)
	if (!ImGui::BeginTabBar("ParticleEditorTabs",
		ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		return;

	// 탭 렌더링
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ViewerState* State = Tabs[i];
		ParticleEditorState* PState = static_cast<ParticleEditorState*>(State);
		bool open = true;

		// 동적 탭 이름 생성
		FString TabDisplayName;
		if (PState->CurrentFilePath.empty())
		{
			TabDisplayName = "Untitled";
		}
		else
		{
			// 파일 경로에서 파일명만 추출 (확장자 제외)
			size_t lastSlash = PState->CurrentFilePath.find_last_of("/\\");
			FString filename = (lastSlash != FString::npos)
				? PState->CurrentFilePath.substr(lastSlash + 1)
				: PState->CurrentFilePath;

			// 확장자 제거
			size_t dotPos = filename.find_last_of('.');
			if (dotPos != FString::npos)
				filename = filename.substr(0, dotPos);

			TabDisplayName = filename;
		}

		// 수정됨 표시 추가
		if (PState->bIsDirty)
		{
			TabDisplayName += "*";
		}

		// ImGui ID 충돌 방지: ##뒤에 고유 식별자 추가 (표시되지 않음)
		TabDisplayName += "##";
		TabDisplayName += State->Name.ToString();

		if (ImGui::BeginTabItem(TabDisplayName.c_str(), &open))
		{
			ActiveTabIndex = i;
			ActiveState = State;
			ImGui::EndTabItem();
		}
		if (!open)
		{
			CloseTab(i);
			ImGui::EndTabBar();
			return;
		}
	}

	// + 버튼 (새 탭 추가)
	if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
	{
		// 내부 식별용 고유 이름 생성 (표시 이름은 RenderTabsAndToolbar에서 동적 생성)
		int maxViewerNum = 0;
		for (int i = 0; i < Tabs.Num(); ++i)
		{
			const FString& tabName = Tabs[i]->Name.ToString();
			const char* prefix = "ParticleTab";
			if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
			{
				const char* numberPart = tabName.c_str() + strlen(prefix);
				int num = atoi(numberPart);
				if (num > maxViewerNum)
				{
					maxViewerNum = num;
				}
			}
		}

		char label[64];
		sprintf_s(label, "ParticleTab%d", maxViewerNum + 1);
		OpenNewTab(label);
	}
	ImGui::EndTabBar();
}

void SParticleEditorWindow::RenderLeftPanel(float PanelWidth)
{
	// 더 이상 사용되지 않음 (새 레이아웃에서는 OnRender에서 직접 처리)
}

void SParticleEditorWindow::RenderRightPanel()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !State->EditingTemplate)
	{
		ImGui::TextDisabled("파티클 시스템이 없습니다");
		return;
	}

	UParticleSystem* System = State->EditingTemplate;

	// 이미터 열 너비 상수
	const float EmitterColumnWidth = 200.f;
	const float EmitterColumnSpacing = 8.0f;

	// 전체 콘텐츠 너비 계산 (이미터 개수 + 새 이미터 추가 여유 공간)
	float TotalContentWidth = (System->Emitters.Num() + 1) * (EmitterColumnWidth + EmitterColumnSpacing);
	ImGui::SetNextWindowContentSize(ImVec2(TotalContentWidth, 0));

	// 가로 스크롤 영역 시작
	ImGui::BeginChild("EmitterScrollArea", ImVec2(0, 0), false,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

	// 각 이미터를 열로 렌더링
	for (int32 i = 0; i < System->Emitters.Num(); ++i)
	{
		UParticleEmitter* Emitter = System->Emitters[i];
		if (!Emitter) continue;

		if (i > 0) ImGui::SameLine();

		ImGui::BeginChild(
			("##Emitter" + std::to_string(i)).c_str(),
			ImVec2(EmitterColumnWidth, 0),
			true,
			ImGuiWindowFlags_NoScrollbar
		);
		{
			RenderEmitterColumn(i, Emitter);
		}
		ImGui::EndChild();
	}

	// 빈 영역 좌클릭 → 선택 해제
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
	{
		State->SelectedEmitterIndex = -1;
		State->SelectedModuleIndex = -1;
		State->SelectedModule = nullptr;
	}

	// 빈 영역 우클릭 → 새 이미터 추가 메뉴
	if (ImGui::BeginPopupContextWindow("EmptyAreaContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("새 파티클 스프라이트 이미터"))
		{
			// 기본 템플릿에서 이미터 가져오기
			UParticleSystem* DefaultTemplate = ParticleEditorBootstrap::CreateDefaultParticleTemplate();
			if (DefaultTemplate && DefaultTemplate->Emitters.Num() > 0)
			{
				UParticleEmitter* NewEmitter = DefaultTemplate->Emitters[0];
				System->Emitters.Add(NewEmitter);

				// EmitterInstances 재생성
				if (State->PreviewComponent)
				{
					State->PreviewComponent->RefreshEmitterInstances();
				}
				State->bIsDirty = true;

				UE_LOG("[ParticleEditor] 새 스프라이트 이미터 추가: %d", System->Emitters.Num() - 1);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

void SParticleEditorWindow::RenderBottomPanel()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State) return;

	// 커브 에디터 위젯에 상태 전달 및 렌더링
	if (CurveEditorWidget)
	{
		CurveEditorWidget->SetEditorState(State);
		CurveEditorWidget->RenderWidget();
	}
}

uint32 SParticleEditorWindow::GetModuleColorInCurveEditor(UParticleModule* Module)
{
	// 커브 에디터 위젯에 위임
	if (CurveEditorWidget)
	{
		return CurveEditorWidget->GetModuleColor(Module);
	}
	return 0;
}

void SParticleEditorWindow::RenderToolbar()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State) return;

	const float IconSize = 36.f;
	const ImVec2 IconSizeVec(IconSize, IconSize);
	const float ToolbarHeight = 48.f;

	// 툴바 영역을 BeginChild로 감싸서 수직 구분선이 전체 높이를 차지하도록 함
	ImGui::BeginChild("##ToolbarArea", ImVec2(0, ToolbarHeight + 6.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	// 버튼 스타일
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));

	// 수직 정렬을 위한 커서 위치 조정
	float cursorY = (ToolbarHeight - IconSize) * 0.5f;
	ImGui::SetCursorPosY(cursorY);

	// ========== 파일 작업 ==========
	// Save 버튼
	if (IconSave && IconSave->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SavePS", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
		{
			SaveParticleSystem();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 저장");
	}

	ImGui::SameLine();

	// SaveAs 버튼
	if (IconSaveAs && IconSaveAs->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SaveAsPS", (void*)IconSaveAs->GetShaderResourceView(), IconSizeVec))
		{
			SaveParticleSystemAs();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("다른 이름으로 저장");
	}

	ImGui::SameLine();

	// Load 버튼
	if (IconLoad && IconLoad->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LoadPS", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
		{
			LoadParticleSystem();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 불러오기");
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// ========== 시뮬레이션 제어 ==========
	// Restart Simulation 버튼
	if (IconRestart && IconRestart->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RestartSim", (void*)IconRestart->GetShaderResourceView(), IconSizeVec))
		{
			if (State->PreviewComponent)
			{
				State->PreviewComponent->ResetParticles();
				FParticleStatManager::GetInstance().ResetStats();
				State->ResetBounds();  // Bounds도 리셋
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("시뮬레이션 재시작");
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// ========== 표시 옵션 ==========
	// Bounds 버튼
	if (IconBounds && IconBounds->GetShaderResourceView())
	{
		// 활성 상태에 따른 스타일 설정
		ImVec4 bgColor = State->bShowBounds
			? ImVec4(0.15f, 0.35f, 0.45f, 0.8f)  // 어두운 청록 배경
			: ImVec4(0, 0, 0, 0);                 // 투명
		ImVec4 tint = State->bShowBounds
			? ImVec4(0.6f, 1.0f, 1.0f, 1.0f)     // 밝은 청록 틴트
			: ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // 회색 (비활성)

		if (ImGui::ImageButton("##Bounds", (void*)IconBounds->GetShaderResourceView(), IconSizeVec, ImVec2(0, 0), ImVec2(1, 1), bgColor, tint))
		{
			State->bShowBounds = !State->bShowBounds;
		}

		// 활성 상태일 때 테두리 추가
		if (State->bShowBounds)
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			ImU32 borderColor = IM_COL32(102, 204, 255, 200);
			draw_list->AddRect(p_min, p_max, borderColor, 3.0f, 0, 2.5f);

			// TODO: 파티클 바운드 그리기
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("파티클 시스템 경계 표시");
	}

	ImGui::SameLine();

	// Origin Axis 버튼
	if (IconOriginAxis && IconOriginAxis->GetShaderResourceView())
	{
		// 활성 상태에 따른 스타일 설정
		ImVec4 bgColor = State->bShowOriginAxis
			? ImVec4(0.15f, 0.35f, 0.45f, 0.8f)  // 어두운 청록 배경
			: ImVec4(0, 0, 0, 0);                 // 투명
		ImVec4 tint = State->bShowOriginAxis
			? ImVec4(0.6f, 1.0f, 1.0f, 1.0f)     // 밝은 청록 틴트
			: ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // 회색 (비활성)

		if (ImGui::ImageButton("##OriginAxis", (void*)IconOriginAxis->GetShaderResourceView(), IconSizeVec, ImVec2(0, 0), ImVec2(1, 1), bgColor, tint))
		{
			State->bShowOriginAxis = !State->bShowOriginAxis;
		}

		// 활성 상태일 때 테두리 추가
		if (State->bShowOriginAxis)
		{
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetItemRectMin();
			ImVec2 p_max = ImGui::GetItemRectMax();
			ImU32 borderColor = IM_COL32(102, 204, 255, 200);
			draw_list->AddRect(p_min, p_max, borderColor, 3.0f, 0, 2.5f);
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("원점 축 표시");
	}

	ImGui::SameLine();

	// Background Color 버튼
	if (IconBackgroundColor && IconBackgroundColor->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##BgColor", (void*)IconBackgroundColor->GetShaderResourceView(), IconSizeVec))
		{
			ImGui::OpenPopup("BackgroundColorPopup");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("배경색 설정");

		// 배경색 팝업
		if (ImGui::BeginPopup("BackgroundColorPopup"))
		{
			float color[3] = { State->BackgroundColor.R, State->BackgroundColor.G, State->BackgroundColor.B };
			if (ImGui::ColorPicker3("배경색", color))
			{
				State->BackgroundColor.R = color[0];
				State->BackgroundColor.G = color[1];
				State->BackgroundColor.B = color[2];
			}
			ImGui::EndPopup();
		}
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// LOD 네비게이션
	// 파티클 시스템의 실제 LOD 개수 가져오기
	int32 MaxLODIndex = 0;
	if (State->EditingTemplate && !State->EditingTemplate->Emitters.IsEmpty())
	{
		UParticleEmitter* Emitter = State->EditingTemplate->Emitters[0];
		MaxLODIndex = Emitter->LODLevels.Num() - 1;
		if (MaxLODIndex < 0) MaxLODIndex = 0;
	}

	// 최하 LOD 버튼
	if (IconLODFirst && IconLODFirst->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODFirst", (void*)IconLODFirst->GetShaderResourceView(), IconSizeVec))
		{
			State->SetLODLevel(0);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("최하 LOD (LOD 0)");
	}

	ImGui::SameLine();

	// 하위 LOD 버튼
	if (IconLODPrev && IconLODPrev->GetShaderResourceView())
	{
		bool bCanGoPrev = State->CurrentLODLevel > 0;
		ImGui::BeginDisabled(!bCanGoPrev);
		if (ImGui::ImageButton("##LODPrev", (void*)IconLODPrev->GetShaderResourceView(), IconSizeVec))
		{
			State->SetLODLevel(State->CurrentLODLevel - 1);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("하위 LOD");
		ImGui::EndDisabled();
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 현재 LOD 드롭다운 메뉴 (수직 중앙 정렬)
	// 드롭다운의 기본 프레임 높이를 고려하여 중앙 정렬
	float comboFrameHeight = ImGui::GetFrameHeight();  // 드롭다운 메뉴의 높이
	float lodCursorY = (ToolbarHeight - comboFrameHeight) * 0.5f;

	ImGui::SetCursorPosY(lodCursorY);
	ImGui::AlignTextToFramePadding();  // 텍스트 수직 정렬
	ImGui::Text("LOD:");

	ImGui::SameLine();
	ImGui::SetCursorPosY(lodCursorY);  // 드롭다운도 같은 Y 위치로
	ImGui::SetNextItemWidth(60.f);

	// LOD 레벨 목록을 드롭다운으로 표시
	if (State->EditingTemplate && !State->EditingTemplate->Emitters.IsEmpty())
	{
		UParticleEmitter* Emitter = State->EditingTemplate->Emitters[0];
		int32 NumLODs = Emitter->LODLevels.Num();

		if (NumLODs > 0)
		{
			// 현재 LOD 범위 체크
			State->CurrentLODLevel = ImClamp(State->CurrentLODLevel, 0, NumLODs - 1);

			// 드롭다운 표시 텍스트
			char CurrentLODText[32];
			sprintf_s(CurrentLODText, "%d", State->CurrentLODLevel);

			if (ImGui::BeginCombo("##CurrentLOD", CurrentLODText))
			{
				for (int32 i = 0; i < NumLODs; i++)
				{
					bool bIsSelected = (State->CurrentLODLevel == i);
					char LODText[32];
					sprintf_s(LODText, "%d", i);

					if (ImGui::Selectable(LODText, bIsSelected))
					{
						State->SetLODLevel(i);
					}

					if (bIsSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			// LOD가 없는 경우
			ImGui::BeginDisabled(true);
			if (ImGui::BeginCombo("##CurrentLOD", "없음"))
			{
				ImGui::EndCombo();
			}
			ImGui::EndDisabled();
		}
	}
	else
	{
		// 파티클 시스템이 없는 경우
		ImGui::BeginDisabled(true);
		if (ImGui::BeginCombo("##CurrentLOD", "없음"))
		{
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 상위 LOD 버튼
	if (IconLODNext && IconLODNext->GetShaderResourceView())
	{
		bool bCanGoNext = State->CurrentLODLevel < MaxLODIndex;
		ImGui::BeginDisabled(!bCanGoNext);
		if (ImGui::ImageButton("##LODNext", (void*)IconLODNext->GetShaderResourceView(), IconSizeVec))
		{
			State->SetLODLevel(State->CurrentLODLevel + 1);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("상위 LOD");
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	// 최상 LOD 버튼
	if (IconLODLast && IconLODLast->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODLast", (void*)IconLODLast->GetShaderResourceView(), IconSizeVec))
		{
			State->SetLODLevel(MaxLODIndex);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("최상 LOD (LOD %d)", MaxLODIndex);
	}

	// 구분선
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	UParticleSystem* System = State->EditingTemplate;

	// LOD 앞에 추가 버튼
	if (IconLODInsertBefore && IconLODInsertBefore->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODInsertBefore", (void*)IconLODInsertBefore->GetShaderResourceView(), IconSizeVec))
		{
			if (System)
			{
				for (UParticleEmitter* Emitter : System->Emitters)
				{
					if (Emitter && State->CurrentLODLevel < Emitter->LODLevels.Num())
					{
						if (UParticleLODLevel* NewLOD = Emitter->DuplicateLODLevel(State->CurrentLODLevel, 1.0f))
							Emitter->InsertLODLevel(State->CurrentLODLevel, NewLOD);
					}
				}
				State->bIsDirty = true;
				if (State->PreviewComponent) State->PreviewComponent->RefreshEmitterInstances();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("현재 LOD 앞에 새 LOD 추가 (복제)");
	}

	ImGui::SameLine();

	// LOD 뒤로 추가 버튼
	if (IconLODInsertAfter && IconLODInsertAfter->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##LODInsertAfter", (void*)IconLODInsertAfter->GetShaderResourceView(), IconSizeVec))
		{
			if (System)
			{
				for (UParticleEmitter* Emitter : System->Emitters)
				{
					if (Emitter && State->CurrentLODLevel < Emitter->LODLevels.Num())
					{
						if (UParticleLODLevel* NewLOD = Emitter->DuplicateLODLevel(State->CurrentLODLevel, 0.75f))
							Emitter->InsertLODLevel(State->CurrentLODLevel + 1, NewLOD);
					}
				}
				State->CurrentLODLevel++;
				State->bIsDirty = true;
				if (State->PreviewComponent) State->PreviewComponent->RefreshEmitterInstances();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("현재 LOD 뒤에 새 LOD 추가 (75%% 스케일)");
	}

	ImGui::SameLine();

	// LOD 삭제 버튼 (최소 1개 유지)
	if (IconLODDelete && IconLODDelete->GetShaderResourceView())
	{
		bool bCanDelete = System != nullptr;
		if (bCanDelete)
		{
			for (UParticleEmitter* Emitter : System->Emitters)
			{
				if (Emitter && Emitter->LODLevels.Num() <= 1) { bCanDelete = false; break; }
			}
		}

		ImGui::BeginDisabled(!bCanDelete);
		if (ImGui::ImageButton("##LODDelete", (void*)IconLODDelete->GetShaderResourceView(), IconSizeVec) && bCanDelete)
		{
			for (UParticleEmitter* Emitter : System->Emitters)
			{
				if (Emitter) Emitter->RemoveLODLevel(State->CurrentLODLevel);
			}
			int32 MaxLOD = System->Emitters.Num() > 0 ? System->Emitters[0]->LODLevels.Num() - 1 : 0;
			if (State->CurrentLODLevel > MaxLOD) State->CurrentLODLevel = MaxLOD;
			State->bIsDirty = true;
			if (State->PreviewComponent) State->PreviewComponent->RefreshEmitterInstances();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(bCanDelete ? "현재 LOD 삭제" : "최소 1개의 LOD는 유지해야 합니다");
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);

	ImGui::EndChild();  // ToolbarArea 종료

	// 프리뷰 컴포넌트에 현재 LOD 레벨 적용
	if (State->PreviewComponent)
	{
		State->PreviewComponent->SetEditorLODLevel(State->CurrentLODLevel);
	}

	ImGui::Separator();
}

void SParticleEditorWindow::RenderDetailsPanel(float PanelWidth)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		ImGui::TextDisabled("파티클 시스템이 로드되지 않았습니다");
		return;
	}

	ImGui::Text("디테일");
	ImGui::Separator();

	// 모듈이 선택된 경우: 모듈 프로퍼티 표시
	if (State->SelectedModule)
	{
		// 모듈 클래스 이름 표시
		UClass* ModuleClass = State->SelectedModule->GetClass();
		const char* DisplayName = ModuleClass->DisplayName;
		ImGui::Text("모듈: %s", DisplayName ? DisplayName : ModuleClass->Name);

		// 프로퍼티 렌더링
		ImGui::PushItemWidth(PanelWidth * 0.55f);
		bool bChanged = UPropertyRenderer::RenderAllPropertiesWithInheritance(State->SelectedModule);
		ImGui::PopItemWidth();

		// 변경 시 Dirty 플래그 및 프리뷰 갱신
		if (bChanged)
		{
			State->bIsDirty = true;
			if (State->PreviewComponent)
			{
				State->PreviewComponent->RefreshEmitterInstances();
			}
		}
		return;
	}

	// 모듈이 선택되지 않은 경우: ParticleSystem LOD 설정 표시
	UParticleSystem* System = State->EditingTemplate;
	if (!System)
	{
		ImGui::TextDisabled("모듈을 선택하세요");
		return;
	}

	ImGui::Text("파티클 시스템 설정");
	ImGui::Separator();

	// LOD 설정 섹션
	ImGui::Text("LOD 설정");
	ImGui::Spacing();

	// bUseLOD 체크박스
	if (ImGui::Checkbox("LOD 사용", &System->bUseLOD))
	{
		State->bIsDirty = true;
	}

	if (System->bUseLOD)
	{
		ImGui::Spacing();

		// 최대 LOD 레벨 수 계산 (모든 이미터 중 최대)
		int32 MaxLODLevels = 0;
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (Emitter)
			{
				MaxLODLevels = FMath::Max(MaxLODLevels, Emitter->LODLevels.Num());
			}
		}

		// LODDistances 배열 크기 자동 조정 (LOD 레벨 수 - 1)
		int32 RequiredDistances = FMath::Max(0, MaxLODLevels - 1);
		while (System->LODDistances.Num() < RequiredDistances)
		{
			// 이전 거리 기반으로 새 거리 계산 (이전 거리 + 50, 없으면 50)
			float PrevDistance = System->LODDistances.Num() > 0
				? System->LODDistances[System->LODDistances.Num() - 1]
				: 0.0f;
			float DefaultDistance = PrevDistance + 50.0f;
			System->LODDistances.Add(DefaultDistance);
			State->bIsDirty = true;
		}
		while (System->LODDistances.Num() > RequiredDistances)
		{
			System->LODDistances.RemoveAt(System->LODDistances.Num() - 1);
			State->bIsDirty = true;
		}

		// LOD 거리 편집 UI
		ImGui::Text("전환 거리 (LOD 레벨: %d개)", MaxLODLevels);
		ImGui::PushItemWidth(PanelWidth * 0.5f);

		for (int32 i = 0; i < System->LODDistances.Num(); i++)
		{
			char Label[64];
			sprintf_s(Label, "LOD %d → %d##LODDist%d", i, i + 1, i);

			// 최소/최대값 계산 (이전 거리 ~ 다음 거리 사이로 제한)
			float MinValue = (i > 0) ? System->LODDistances[i - 1] + 1.0f : 0.0f;
			float MaxValue = (i + 1 < System->LODDistances.Num()) ? System->LODDistances[i + 1] - 1.0f : 50000.0f;

			if (ImGui::DragFloat(Label, &System->LODDistances[i], 10.0f, MinValue, MaxValue, "%.0f"))
			{
				// 더블클릭 직접 입력 시 범위 벗어날 수 있으므로 클램핑
				System->LODDistances[i] = FMath::Clamp(System->LODDistances[i], MinValue, MaxValue);
				State->bIsDirty = true;
			}
		}

		ImGui::PopItemWidth();
	}
}

// 일반 모듈 추가 헬퍼 함수 (LOD 0에서 추가 시 모든 LOD에 동기화)
template<typename T>
static void AddModuleToLOD(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	// LOD 0에만 추가 (모든 LOD에 동기화)
	UParticleEmitter* Emitter = State->EditingTemplate ?
		(State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < State->EditingTemplate->Emitters.Num() ?
			State->EditingTemplate->Emitters[State->SelectedEmitterIndex] : nullptr) : nullptr;

	if (Emitter)
	{
		// 모든 LOD에 동일한 모듈 타입 추가
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (LODLevel)
			{
				T* NewModule = NewObject<T>();
				LODLevel->Modules.Add(NewModule);
				LODLevel->CacheModuleInfo();
			}
		}
	}
	else
	{
		// 폴백: 단일 LOD에만 추가
		T* NewModule = NewObject<T>();
		LOD->Modules.Add(NewModule);
		LOD->CacheModuleInfo();
	}

	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 타입 데이터 모듈 설정 헬퍼 함수
// TODO: 타입 데이터 모듈 별로 다시 만들어줘야함.
// 예: SetMeshTypeDataModule()
template<typename T>
static void SetTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	// 기존 타입 데이터가 있으면 제거 및 삭제
	if (LOD->TypeDataModule)
	{
		LOD->Modules.Remove(LOD->TypeDataModule);
		DeleteObject(LOD->TypeDataModule);
		LOD->TypeDataModule = nullptr;
	}

	// 새 타입 데이터 생성 및 설정
	T* NewModule = NewObject<T>();
	LOD->TypeDataModule = NewModule;
	LOD->Modules.Add(NewModule);
	LOD->CacheModuleInfo();
	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 메시 타입 데이터 모듈 설정 (기본 메시 포함, 모든 LOD에 동기화)
static void SetMeshTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	UParticleEmitter* Emitter = State->EditingTemplate ?
		(State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < State->EditingTemplate->Emitters.Num() ?
			State->EditingTemplate->Emitters[State->SelectedEmitterIndex] : nullptr) : nullptr;

	auto ApplyToLOD = [](UParticleLODLevel* TargetLOD) {
		// 기존 타입 데이터가 있으면 제거 및 삭제
		if (TargetLOD->TypeDataModule)
		{
			TargetLOD->Modules.Remove(TargetLOD->TypeDataModule);
			DeleteObject(TargetLOD->TypeDataModule);
			TargetLOD->TypeDataModule = nullptr;
		}

		// 메시 타입 데이터 생성
		UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();
		MeshTypeData->Mesh = UResourceManager::GetInstance().Load<UStaticMesh>(GDataDir + "/cube-tex.obj");

		// RequiredModule->Material에 메시 파티클용 셰이더 설정
		if (TargetLOD->RequiredModule)
		{
			UMaterialInterface* OldMaterial = TargetLOD->RequiredModule->Material;
			if (OldMaterial && OldMaterial->GetFilePath().empty())
			{
				DeleteObject(OldMaterial);
			}

			UMaterial* MeshMaterial = NewObject<UMaterial>();
			UShader* MeshShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleMesh.hlsl");
			MeshMaterial->SetShader(MeshShader);
			TargetLOD->RequiredModule->Material = MeshMaterial;
			TargetLOD->RequiredModule->bOwnsMaterial = true;
		}

		TargetLOD->TypeDataModule = MeshTypeData;
		TargetLOD->Modules.Add(MeshTypeData);
		TargetLOD->CacheModuleInfo();
	};

	// 모든 LOD에 적용
	if (Emitter)
	{
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (LODLevel) ApplyToLOD(LODLevel);
		}
	}
	else
	{
		ApplyToLOD(LOD);
	}

	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 빔 타입 데이터 모듈 설정 (모든 LOD에 동기화)
static void SetBeamTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	UParticleEmitter* Emitter = State->EditingTemplate ?
		(State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < State->EditingTemplate->Emitters.Num() ?
			State->EditingTemplate->Emitters[State->SelectedEmitterIndex] : nullptr) : nullptr;

	auto ApplyToLOD = [](UParticleLODLevel* TargetLOD) {
		if (TargetLOD->TypeDataModule)
		{
			TargetLOD->Modules.Remove(TargetLOD->TypeDataModule);
			DeleteObject(TargetLOD->TypeDataModule);
			TargetLOD->TypeDataModule = nullptr;
		}

		UParticleModuleTypeDataBeam* BeamTypeData = NewObject<UParticleModuleTypeDataBeam>();

		if (TargetLOD->RequiredModule)
		{
			UMaterialInterface* OldMaterial = TargetLOD->RequiredModule->Material;
			if (OldMaterial && OldMaterial->GetFilePath().empty())
			{
				DeleteObject(OldMaterial);
			}

			UMaterial* BeamMaterial = NewObject<UMaterial>();
			UShader* BeamShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleBeam.hlsl");
			BeamMaterial->SetShader(BeamShader);
			TargetLOD->RequiredModule->Material = BeamMaterial;
			TargetLOD->RequiredModule->bOwnsMaterial = true;
		}

		TargetLOD->TypeDataModule = BeamTypeData;
		TargetLOD->Modules.Add(BeamTypeData);
		TargetLOD->CacheModuleInfo();
	};

	if (Emitter)
	{
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (LODLevel) ApplyToLOD(LODLevel);
		}
	}
	else
	{
		ApplyToLOD(LOD);
	}

	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

// 리본 타입 데이터 모듈 설정 (모든 LOD에 동기화)
static void SetRibbonTypeDataModule(UParticleLODLevel* LOD, ParticleEditorState* State)
{
	UParticleEmitter* Emitter = State->EditingTemplate ?
		(State->SelectedEmitterIndex >= 0 && State->SelectedEmitterIndex < State->EditingTemplate->Emitters.Num() ?
			State->EditingTemplate->Emitters[State->SelectedEmitterIndex] : nullptr) : nullptr;

	auto ApplyToLOD = [](UParticleLODLevel* TargetLOD) {
		if (TargetLOD->TypeDataModule)
		{
			TargetLOD->Modules.Remove(TargetLOD->TypeDataModule);
			DeleteObject(TargetLOD->TypeDataModule);
			TargetLOD->TypeDataModule = nullptr;
		}

		UParticleModuleTypeDataRibbon* RibbonTypeData = NewObject<UParticleModuleTypeDataRibbon>();
		RibbonTypeData->RibbonWidth = 1.0f;

		if (TargetLOD->RequiredModule)
		{
			UMaterialInterface* OldMaterial = TargetLOD->RequiredModule->Material;
			if (OldMaterial && OldMaterial->GetFilePath().empty())
			{
				DeleteObject(OldMaterial);
			}

			UMaterial* RibbonMaterial = NewObject<UMaterial>();
			UShader* RibbonShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Particle/ParticleRibbon.hlsl");
			RibbonMaterial->SetShader(RibbonShader);
			TargetLOD->RequiredModule->Material = RibbonMaterial;
			TargetLOD->RequiredModule->bOwnsMaterial = true;
		}

		TargetLOD->TypeDataModule = RibbonTypeData;
		TargetLOD->Modules.Add(RibbonTypeData);
		TargetLOD->CacheModuleInfo();
	};

	if (Emitter)
	{
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (LODLevel) ApplyToLOD(LODLevel);
		}
	}
	else
	{
		ApplyToLOD(LOD);
	}

	if (State->PreviewComponent)
	{
		State->PreviewComponent->RefreshEmitterInstances();
	}
	State->bIsDirty = true;
}

void SParticleEditorWindow::RenderEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !Emitter) return;

	int32 LODIndex = State->CurrentLODLevel;
	UParticleLODLevel* LOD = Emitter->GetLODLevel(LODIndex);
	if (!LOD)
	{
		ImGui::TextDisabled("LOD %d 없음", LODIndex);
		return;
	}

	// 이미터가 선택되었거나 해당 이미터의 모듈이 선택된 경우 하이라이트
	bool bEmitterSelected = (State->SelectedEmitterIndex == EmitterIndex);

	// 헤더 배경색 (선택: 연보라색, 기본: 청회색)
	ImVec4 HeaderBgColor = bEmitterSelected
		? ImVec4(0.25f, 0.2f, 0.5f, 1.0f)
		: ImVec4(0.20f, 0.22f, 0.28f, 1.0f);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, HeaderBgColor);
	ImGui::BeginChild(("##EmitterHeader" + std::to_string(EmitterIndex)).c_str(), ImVec2(0, 70), false);

	ImDrawList* HeaderDrawList = ImGui::GetWindowDrawList();
	ImVec2 WindowPos = ImGui::GetWindowPos();

	// 첫 번째 줄: "Particle Emitter" 텍스트
	const char* HeaderText = "Particle Emitter";
	ImVec2 TextPos(WindowPos.x + 5.0f, WindowPos.y + 3.0f);
	HeaderDrawList->AddText(ImVec2(TextPos.x + 1, TextPos.y + 1), IM_COL32(0, 0, 0, 180), HeaderText);  // 그림자
	HeaderDrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), HeaderText);  // 본문

	// 두 번째 줄: 체크박스 + 파티클 개수
	ImGui::SetCursorPosY(ImGui::GetTextLineHeightWithSpacing() + 5.0f);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);
	ImGui::BeginGroup();

	// 커스텀 체크박스 (체크/X 토글)
	ImVec2 CheckboxSize(14, 14);
	ImVec2 CheckboxPos = ImGui::GetCursorScreenPos();

	// 체크박스 배경 (밝은 색상)
	ImU32 CheckboxBgColor = LOD->bEnabled ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
	HeaderDrawList->AddRectFilled(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), CheckboxBgColor);
	HeaderDrawList->AddRect(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), IM_COL32(60, 60, 60, 255));

	// 체크 또는 X 표시
	if (LOD->bEnabled)
	{
		// 체크 표시
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 7),
			ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
			IM_COL32(255, 255, 255, 255), 2.0f);
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 4),
			IM_COL32(255, 255, 255, 255), 2.0f);
	}
	else
	{
		// X 표시
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 3),
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 11),
			IM_COL32(255, 255, 255, 255), 2.0f);
		HeaderDrawList->AddLine(
			ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 3),
			ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 11),
			IM_COL32(255, 255, 255, 255), 2.0f);
	}

	// 클릭 감지용 InvisibleButton
	char CheckboxId[32];
	sprintf_s(CheckboxId, "##EmitterEnabled%d", EmitterIndex);
	if (ImGui::InvisibleButton(CheckboxId, CheckboxSize))
	{
		LOD->bEnabled = !LOD->bEnabled;
		State->bIsDirty = true;
	}

	ImGui::SameLine();

	// 파티클 개수 표시 (그림자 효과)
	int32 ParticleCount = 0;
	if (State->PreviewComponent && EmitterIndex < State->PreviewComponent->EmitterInstances.Num())
	{
		FParticleEmitterInstance* Instance = State->PreviewComponent->EmitterInstances[EmitterIndex];
		if (Instance)
		{
			ParticleCount = Instance->ActiveParticles;
		}
	}
	char CountText[32];
	sprintf_s(CountText, "%d", ParticleCount);
	ImVec2 CountPos = ImGui::GetCursorScreenPos();
	CountPos.y -= 3.0f;  // 체크박스와 높이 정렬
	HeaderDrawList->AddText(ImVec2(CountPos.x + 1, CountPos.y + 1), IM_COL32(0, 0, 0, 180), CountText);  // 그림자
	HeaderDrawList->AddText(CountPos, IM_COL32(255, 255, 255, 255), CountText);  // 본문

	ImGui::EndGroup();

	// 헤더 클릭 감지 (전체 영역) - 이미터 헤더 + 필수 모듈 선택
	ImVec2 HeaderMin = ImGui::GetWindowPos();
	ImVec2 HeaderMax = ImVec2(HeaderMin.x + ImGui::GetWindowWidth(), HeaderMin.y + 70);
	if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(HeaderMin, HeaderMax))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModule = LOD->RequiredModule;
		State->SelectedModuleIndex = LOD->RequiredModule ? 1 : -1;  // Required 모듈의 표시 우선순위는 1
	}

	// 썸네일 영역 (우측 상단에 배치)
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 55, 5));
	ImVec2 ThumbnailPos = ImGui::GetCursorScreenPos();
	ImVec2 ThumbnailSize(50, 50);

	// RequiredModule의 Material에서 Diffuse 텍스처 가져오기
	ID3D11ShaderResourceView* DiffuseSRV = nullptr;
	if (LOD->RequiredModule && LOD->RequiredModule->Material)
	{
		UTexture* DiffuseTexture = LOD->RequiredModule->Material->GetTexture(EMaterialTextureSlot::Diffuse);
		if (DiffuseTexture)
		{
			DiffuseSRV = DiffuseTexture->GetShaderResourceView();
		}
	}

	// 썸네일 검은색 배경
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		ThumbnailPos,
		ImVec2(ThumbnailPos.x + ThumbnailSize.x, ThumbnailPos.y + ThumbnailSize.y),
		IM_COL32(0, 0, 0, 255)
	);

	if (DiffuseSRV)
	{
		// Diffuse 텍스처 표시
		ImGui::Image((void*)DiffuseSRV, ThumbnailSize);
	}
	else
	{
		ImGui::Dummy(ThumbnailSize);
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Separator();

	// 스크롤 가능한 모듈 영역
	ImGui::BeginChild(
		("##ModuleList" + std::to_string(EmitterIndex)).c_str(),
		ImVec2(0, 0),
		false,
		ImGuiWindowFlags_None
	);

	int32 ModuleIdx = 0;

	// 모듈을 우선순위에 따라 정렬하여 렌더링
	// 우선순위: TypeData(0) -> Required(1) -> Spawn(2) -> 일반(100)
	TArray<int32> SortedIndices;
	for (int32 i = 0; i < LOD->Modules.Num(); ++i)
	{
		SortedIndices.Add(i);
	}

	// 우선순위 기준 정렬
	std::sort(SortedIndices.begin(), SortedIndices.end(), [&LOD](int32 A, int32 B)
	{
		UParticleModule* ModuleA = LOD->Modules[A];
		UParticleModule* ModuleB = LOD->Modules[B];
		int32 PriorityA = ModuleA ? ModuleA->GetDisplayPriority() : 100;
		int32 PriorityB = ModuleB ? ModuleB->GetDisplayPriority() : 100;
		return PriorityA < PriorityB;
	});

	// 정렬된 순서로 렌더링
	for (int32 SortedIdx : SortedIndices)
	{
		UParticleModule* Module = LOD->Modules[SortedIdx];
		if (Module)
		{
			RenderModuleBlock(EmitterIndex, ModuleIdx++, Module);
		}
	}

	// 모듈 리스트 빈 영역 좌클릭 → 이미터 헤더 + 필수 모듈 선택
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModule = LOD->RequiredModule;
		State->SelectedModuleIndex = LOD->RequiredModule ? 1 : -1;  // Required 모듈의 표시 우선순위는 1
	}

	// 이미터 영역 우클릭 → 모듈 추가/이미터 관리 메뉴
	char ContextMenuId[64];
	sprintf_s(ContextMenuId, "EmitterContextMenu%d", EmitterIndex);

	if (ImGui::BeginPopupContextWindow(ContextMenuId, ImGuiPopupFlags_MouseButtonRight))
	{
		// LOD 0이 아닌 경우 모듈 추가/삭제 비활성화 안내
		bool bIsLOD0 = (State->CurrentLODLevel == 0);
		if (!bIsLOD0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "LOD 0에서만 모듈 구성 변경 가능");
			ImGui::Separator();
		}

		// 이미터 삭제
		if (ImGui::MenuItem("이미터 삭제"))
		{
			UParticleSystem* System = State->EditingTemplate;
			if (System && EmitterIndex < System->Emitters.Num())
			{
				System->Emitters.RemoveAt(EmitterIndex);
				State->SelectedEmitterIndex = -1;
				State->SelectedModuleIndex = -1;
				State->SelectedModule = nullptr;
				State->bIsDirty = true;
				if (State->PreviewComponent)
				{
					State->PreviewComponent->RefreshEmitterInstances();
				}
			}
		}

		ImGui::Separator();

		// LOD 0이 아닐 때 모듈 추가 메뉴 비활성화
		ImGui::BeginDisabled(!bIsLOD0);

		// 타입 데이터 (스프라이트가 아닌 경우에만 변경 가능)
		// 스프라이트 타입데이터는 기본값이므로 메뉴에 표시하지 않음
		bool bHasNonSpriteTypeData = LOD->TypeDataModule && !Cast<UParticleModuleTypeDataSprite>(LOD->TypeDataModule);
		if (ImGui::BeginMenu("타입 데이터"))
		{
			if (ImGui::MenuItem("메시"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetMeshTypeDataModule(LOD, State);
				}
			}
			if (ImGui::MenuItem("빔"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetBeamTypeDataModule(LOD, State);
				}
			}
			if (ImGui::MenuItem("리본"))
			{
				if (bHasNonSpriteTypeData)
				{
					bShowTypeDataExistsPopup = true;
				}
				else
				{
					SetRibbonTypeDataModule(LOD, State);
				}
			}
			ImGui::EndMenu();
		}

		// 가속
		if (ImGui::BeginMenu("가속"))
		{
			if (ImGui::MenuItem("가속"))
			{
				AddModuleToLOD<UParticleModuleAcceleration>(LOD, State);
			}
			ImGui::EndMenu();
		}
		// 콜리전
		if (ImGui::BeginMenu("콜리전"))
		{
			if (ImGui::MenuItem("콜리전"))
			{
				AddModuleToLOD<UParticleModuleCollision>(LOD, State);
			}
			ImGui::EndMenu();
		}
		// 컬러
		if (ImGui::BeginMenu("컬러"))
		{
			if (ImGui::MenuItem("컬러 오버 라이프"))
			{
				AddModuleToLOD<UParticleModuleColor>(LOD, State);
			}
			ImGui::EndMenu();
		}
		// 이벤트
		if (ImGui::BeginMenu("이벤트"))
		{
			if (ImGui::MenuItem("이벤트 제네레이터"))
			{
				AddModuleToLOD<UParticleModuleEventGenerator>(LOD, State);
			}
			if (ImGui::MenuItem("이벤트 리시버 킬"))
			{
				AddModuleToLOD<UParticleModuleEventReceiverKill>(LOD, State);
			}
			if (ImGui::MenuItem("이벤트 리시버 스폰"))
			{
				AddModuleToLOD<UParticleModuleEventReceiverSpawn>(LOD, State);
			}

			ImGui::EndMenu();
		}
		// 수명
		if (ImGui::BeginMenu("수명"))
		{
			if (ImGui::MenuItem("수명"))
			{
				AddModuleToLOD<UParticleModuleLifetime>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 위치
		if (ImGui::BeginMenu("위치"))
		{
			if (ImGui::MenuItem("초기 위치"))
			{
				AddModuleToLOD<UParticleModuleLocation>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 회전
		// 메시 타입 데이터인 경우 메시 회전만, 그 외에는 초기 회전만 표시
		bool bIsMeshTypeData = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule) != nullptr;
		if (ImGui::BeginMenu("회전"))
		{
			if (bIsMeshTypeData)
			{
				if (ImGui::MenuItem("메시 회전"))
				{
					AddModuleToLOD<UParticleModuleMeshRotation>(LOD, State);
				}
			}
			else
			{
				if (ImGui::MenuItem("초기 회전"))
				{
					AddModuleToLOD<UParticleModuleRotation>(LOD, State);
				}
			}
			ImGui::EndMenu();
		}

		// 회전 속도
		if (ImGui::BeginMenu("회전 속도"))
		{
			if (ImGui::MenuItem("초기 회전 속도"))
			{
				AddModuleToLOD<UParticleModuleRotationRate>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 크기
		if (ImGui::BeginMenu("크기"))
		{
			if (ImGui::MenuItem("초기 크기"))
			{
				AddModuleToLOD<UParticleModuleSize>(LOD, State);
			}
			if (ImGui::MenuItem("속도 기준 크기"))
			{
				AddModuleToLOD<UParticleModuleSizeScaleBySpeed>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 속도
		if (ImGui::BeginMenu("속도"))
		{
			if (ImGui::MenuItem("초기 속도"))
			{
				AddModuleToLOD<UParticleModuleVelocity>(LOD, State);
			}
			ImGui::EndMenu();
		}

		// 트레일
		if (ImGui::BeginMenu("트레일"))
		{
			if (ImGui::MenuItem("트레일 소스"))
			{
				AddModuleToLOD<UParticleModuleTrailSource>(LOD, State);
			}
			ImGui::EndMenu();
		}

		ImGui::EndDisabled();  // LOD 0 체크 종료

		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

void SParticleEditorWindow::RenderModuleBlock(int32 EmitterIdx, int32 ModuleIdx, UParticleModule* Module)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !Module) return;

	// 모듈 표시 이름 가져오기
	FString DisplayName = Module->GetClass()->DisplayName ? Module->GetClass()->DisplayName : "";
	if (DisplayName.empty())
	{
		DisplayName = Module->GetClass()->Name ? Module->GetClass()->Name : "Unknown";
		// "UParticleModule" 접두사 제거
		const FString Prefix = "UParticleModule";
		if (DisplayName.find(Prefix) == 0)
		{
			DisplayName = DisplayName.substr(Prefix.length());
		}

		// 언리얼 엔진 표준 모듈 이름 매핑
		static const std::unordered_map<FString, FString> ModuleNameMap = {
			{"Color", "컬러 오버 라이프"},
			{"Size", "초기 크기"},
			{"Velocity", "초기 속도"},
			{"Lifetime", "수명"},
			{"Spawn", "스폰"},
			{"Required", "필수"},
			{"TypeDataSprite", "스프라이트 타입데이터"},
			{"TypeDataMesh", "메시 타입데이터"},
			{"TypeDataRibbon", "리본 타입데이터"},
			{"TypeDataBeam", "빔 타입데이터"},
			{"Acceleration", "가속"},
			{"Location", "초기 위치"},
			{"Rotation", "초기 회전"},
			{"MeshRotation", "메시 회전"},
			{"RotationRate", "초기 회전 속도"},
			{"SizeScaleBySpeed", "속도 기준 크기"},
			{"Collision", "콜리전"},
			{"EventGenerator", "이벤트 제네레이터"},
			{"EventReceiverKill", "이벤트 리시버 킬"},
			{"EventReceiverSpawn", "이벤트 리시버 스폰"},
			{"TrailSource", "트레일 소스"}
		};

		auto it = ModuleNameMap.find(DisplayName);
		if (it != ModuleNameMap.end())
		{
			DisplayName = it->second;
		}
	}

	// 클래스 이름 (색상 결정용)
	FString ClassName = Module->GetClass()->Name ? Module->GetClass()->Name : "";

	// 선택 상태 확인
	bool bSelected = (State->SelectedEmitterIndex == EmitterIdx &&
					  State->SelectedModule == Module);

	// LOD 및 이동 가능 여부 확인
	UParticleLODLevel* LOD = nullptr;
	bool bCanMove = false;
	if (State->EditingTemplate && EmitterIdx < State->EditingTemplate->Emitters.Num())
	{
		UParticleEmitter* Emitter = State->EditingTemplate->Emitters[EmitterIdx];
		if (Emitter)
		{
			LOD = Emitter->GetLODLevel(State->CurrentLODLevel);
			if (LOD)
			{
				// Required, Spawn, TypeData 모듈은 이동 불가
				bCanMove = (Module != LOD->RequiredModule) &&
						   (Module != LOD->SpawnModule) &&
						   (Module != LOD->TypeDataModule);
			}
		}
	}

	// 모듈 타입에 따른 배경색
	ImVec4 ModuleBgColor;
	if (bSelected)
	{
		ModuleBgColor = ImVec4(0.25f, 0.2f, 0.5f, 1.0f);  // 선택: 연보라색
	}
	else if (ClassName.find("TypeData") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // TypeData: 검정
	}
	else if (ClassName.find("Required") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.6f, 0.6f, 0.2f, 1.0f);  // Required: 노랑
	}
	else if (ClassName.find("Spawn") != std::string::npos)
	{
		ModuleBgColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);  // Spawn: 빨강
	}
	else
	{
		ModuleBgColor = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);  // 기본: 회색
	}

	// 비활성화 시 어두운 갈색 계열로 변경
	if (!Module->bEnabled)
	{
		ModuleBgColor = ImVec4(0.2f, 0.15f, 0.1f, 1.0f);  // 비활성화: 어두운 갈색
	}

	// 모듈 블록 높이
	const float ModuleHeight = 24.0f;

	// TypeData, Required가 아닌 경우에만 체크박스 표시
	bool bShowCheckbox = (ClassName.find("TypeData") == std::string::npos) &&
						 (ClassName.find("Required") == std::string::npos);

	// 모듈 배경 그리기
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModuleBgColor);
	char ModuleChildId[64];
	sprintf_s(ModuleChildId, "##ModuleBlock_%d_%d", EmitterIdx, ModuleIdx);
	ImGui::BeginChild(ModuleChildId, ImVec2(0, ModuleHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	// 스프라이트 타입데이터는 선택 불가
	bool bIsSpriteTypeData = Cast<UParticleModuleTypeDataSprite>(Module) != nullptr;

	// 드래그 가능 영역 (체크박스 제외)
	float DragAreaWidth = bShowCheckbox ? (ImGui::GetWindowWidth() - 42.0f) : ImGui::GetWindowWidth();
	char DragAreaId[64];
	sprintf_s(DragAreaId, "##ModuleDragArea_%d_%d", EmitterIdx, ModuleIdx);
	ImGui::InvisibleButton(DragAreaId, ImVec2(DragAreaWidth, ModuleHeight));

	// 클릭 감지
	if (ImGui::IsItemClicked(0) && !bIsSpriteTypeData)
	{
		State->SelectedEmitterIndex = EmitterIdx;
		State->SelectedModuleIndex = ModuleIdx;
		State->SelectedModule = Module;
	}

	// 드래그 소스 (이동 가능한 모듈만)
	if (bCanMove && ImGui::BeginDragDropSource())
	{
		FModuleDragPayload Payload;
		Payload.EmitterIndex = EmitterIdx;
		Payload.Module = Module;
		ImGui::SetDragDropPayload("MODULE_REORDER", &Payload, sizeof(FModuleDragPayload));
		ImGui::Text("모듈 이동: %s", DisplayName.c_str());
		ImGui::EndDragDropSource();
	}

	// 모듈 이름
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 WindowPos = ImGui::GetWindowPos();

	// 커브 에디터에 있는 모듈이면 왼쪽에 색상 바 표시
	uint32 ModuleCurveColor = GetModuleColorInCurveEditor(Module);
	const float ColorBarWidth = 4.0f;
	float TextOffsetX = 5.0f;
	if (ModuleCurveColor != 0)
	{
		DrawList->AddRectFilled(
			ImVec2(WindowPos.x, WindowPos.y),
			ImVec2(WindowPos.x + ColorBarWidth, WindowPos.y + ModuleHeight),
			ModuleCurveColor);
		TextOffsetX = ColorBarWidth + 4.0f;  // 색상 바 오른쪽에 여백
	}

	float TextY = WindowPos.y + (ModuleHeight - ImGui::GetTextLineHeight()) * 0.5f;
	ImVec2 ModuleTextPos(WindowPos.x + TextOffsetX, TextY);
	DrawList->AddText(ImVec2(ModuleTextPos.x + 1, ModuleTextPos.y + 1), IM_COL32(0, 0, 0, 180), DisplayName.c_str());  // 그림자
	DrawList->AddText(ModuleTextPos, IM_COL32(255, 255, 255, 255), DisplayName.c_str());  // 본문

	// 체크박스 (모듈 블록 내부에서 그리기)
	if (bShowCheckbox)
	{
		float CheckboxX = ImGui::GetWindowWidth() - 38.0f;
		float CheckboxY = (ModuleHeight - 14.0f) * 0.5f;

		ImGui::SetCursorPos(ImVec2(CheckboxX, CheckboxY));

		ImVec2 CheckboxSize(14, 14);
		ImVec2 CheckboxPos = ImGui::GetCursorScreenPos();

		ImU32 CheckboxBgColor = Module->bEnabled ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
		DrawList->AddRectFilled(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), CheckboxBgColor);
		DrawList->AddRect(CheckboxPos, ImVec2(CheckboxPos.x + CheckboxSize.x, CheckboxPos.y + CheckboxSize.y), IM_COL32(60, 60, 60, 255));

		if (Module->bEnabled)
		{
			// 체크 표시
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 7),
				ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
				IM_COL32(255, 255, 255, 255), 2.0f);
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 6, CheckboxPos.y + 10),
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 4),
				IM_COL32(255, 255, 255, 255), 2.0f);
		}
		else
		{
			// X 표시
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 3),
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 11),
				IM_COL32(255, 255, 255, 255), 2.0f);
			DrawList->AddLine(
				ImVec2(CheckboxPos.x + 11, CheckboxPos.y + 3),
				ImVec2(CheckboxPos.x + 3, CheckboxPos.y + 11),
				IM_COL32(255, 255, 255, 255), 2.0f);
		}

		char CheckboxId[64];
		sprintf_s(CheckboxId, "##ModuleEnabled_%d_%d", EmitterIdx, ModuleIdx);
		if (ImGui::InvisibleButton(CheckboxId, CheckboxSize))
		{
			Module->bEnabled = !Module->bEnabled;
			State->bIsDirty = true;
		}

		// 커브 아이콘 (Distribution 커브가 있는 모듈만, 체크박스 오른쪽에)
		if (HasCurveProperties(Module) && IconCurve && IconCurve->GetShaderResourceView())
		{
			float CurveIconX = CheckboxX + 19.0f;  // 체크박스 오른쪽에 4px 간격
			ImVec2 CurveIconSize(14, 14);
			ImVec2 CurveIconPos = ImVec2(WindowPos.x + CurveIconX, WindowPos.y + CheckboxY);

			// 모듈이 커브 에디터에 추가되어 있는지 확인
			bool bIsInCurveEditor = CurveEditorWidget ? CurveEditorWidget->HasModule(Module) : false;

			// 추가된 상태면 노란 테두리 표시
			if (bIsInCurveEditor)
			{
				DrawList->AddRect(
					ImVec2(CurveIconPos.x - 1, CurveIconPos.y - 1),
					ImVec2(CurveIconPos.x + CurveIconSize.x + 1, CurveIconPos.y + CurveIconSize.y + 1),
					IM_COL32(255, 200, 0, 255), 0.0f, 0, 2.0f);
			}

			// 이미지 직접 그리기 (호버 효과 없음)
			DrawList->AddImage(
				(ImTextureID)IconCurve->GetShaderResourceView(),
				CurveIconPos,
				ImVec2(CurveIconPos.x + CurveIconSize.x, CurveIconPos.y + CurveIconSize.y));

			// 클릭 감지용 InvisibleButton
			ImGui::SetCursorPos(ImVec2(CurveIconX, CheckboxY));
			char CurveIconId[64];
			sprintf_s(CurveIconId, "##CurveIcon_%d_%d", EmitterIdx, ModuleIdx);
			if (ImGui::InvisibleButton(CurveIconId, CurveIconSize))
			{
				ToggleCurveTrack(Module);  // 토글: 추가/제거
			}
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();

	// Child 윈도우 전체를 드롭 타겟으로 설정 (Required/Spawn/TypeData도 드롭 대상이 될 수 있도록)
	if (LOD && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("MODULE_REORDER"))
		{
			FModuleDragPayload* DragPayload = (FModuleDragPayload*)Payload->Data;

			// 같은 이미터 내에서만 이동 가능
			if (DragPayload->EmitterIndex == EmitterIdx)
			{
				UParticleModule* DraggedModule = DragPayload->Module;

				// Modules 배열에서 드래그된 모듈의 인덱스 찾기
				int32 SourceIdx = LOD->Modules.Find(DraggedModule);
				int32 TargetIdx = LOD->Modules.Find(Module);

				// 타겟이 Required/Spawn/TypeData가 아닌 경우에만 (Modules 배열에 있는 경우)
				if (SourceIdx >= 0 && TargetIdx >= 0 && SourceIdx != TargetIdx)
				{
					// 원래 타겟 인덱스 기억 (Remove 전)
					int32 InsertIdx = TargetIdx;

					// 모듈 순서 변경
					LOD->Modules.Remove(DraggedModule);

					// 아래로 이동 시: 원래 타겟 위치에 삽입 (타겟 뒤로 이동)
					// 위로 이동 시: 원래 타겟 위치에 삽입 (타겟 자리로 이동)
					if (InsertIdx > LOD->Modules.Num())
					{
						InsertIdx = LOD->Modules.Num();
					}
					LOD->Modules.Insert(DraggedModule, InsertIdx);

					LOD->CacheModuleInfo();
					State->bIsDirty = true;

					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 모듈 우클릭 컨텍스트 메뉴
	char ContextMenuId[64];
	sprintf_s(ContextMenuId, "ModuleContextMenu_%d_%d", EmitterIdx, ModuleIdx);

	if (ImGui::BeginPopupContextItem(ContextMenuId))
	{
		// 삭제 (Required, Spawn, 스프라이트 타입데이터 모듈은 삭제 불가)
		// LOD는 이미 위에서 구함
		bool bCanDelete = false;
		if (LOD)
		{
			bCanDelete = (Module != LOD->RequiredModule) &&
						 (Module != LOD->SpawnModule) &&
						 !Cast<UParticleModuleTypeDataSprite>(Module);
		}

		if (bCanDelete)
		{
			if (ImGui::MenuItem("모듈 삭제"))
			{
				if (LOD)
				{
					// TypeDataModule인 경우 (메시/리본/빔 삭제 시 스프라이트로 복원)
					if (Module == LOD->TypeDataModule)
					{
						RestoreToSpriteTypeData(LOD, State);
					}
					else
					{
						// 일반 모듈 배열에서 삭제
						LOD->Modules.Remove(Module);
						LOD->CacheModuleInfo();
					}

					State->SelectedModule = nullptr;
					State->SelectedModuleIndex = -1;
					State->bIsDirty = true;
					if (State->PreviewComponent)
					{
						State->PreviewComponent->RefreshEmitterInstances();
					}
				}
			}
		}
		else
		{
			ImGui::TextDisabled("(삭제 불가)");
		}

		ImGui::EndPopup();
	}
}

void SParticleEditorWindow::RenderViewportArea(float width, float height)
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State || !State->Viewport)
	{
		ImGui::TextDisabled("뷰포트 없음");
		return;
	}

	// CenterRect 업데이트 (뷰포트 영역)
	ImVec2 Pos = ImGui::GetCursorScreenPos();
	CenterRect.Left = Pos.x;
	CenterRect.Top = Pos.y;
	CenterRect.Right = Pos.x + width;
	CenterRect.Bottom = Pos.y + height;
	CenterRect.UpdateMinMax();

	// 뷰포트 업데이트 및 렌더링
	PreRenderViewportUpdate();
	OnRenderViewport();

	// 렌더링된 텍스처 표시
	ID3D11ShaderResourceView* ViewportSRV = State->Viewport->GetSRV();
	if (ViewportSRV)
	{
		ImVec2 ViewportSize(width, height);
		ImVec2 uv0(0, 0);
		ImVec2 uv1(1, 1);

		ImGui::Image((void*)ViewportSRV, ViewportSize, uv0, uv1);

		// 뷰포트 hover 상태 저장 (다른 UI 추가 전에 체크해야 함)
		bool bViewportHovered = ImGui::IsItemHovered();

		// === 좌상단: 뷰 옵션 버튼 ===
		ImVec2 ViewButtonPos(Pos.x + 10.0f, Pos.y + 10.0f);
		ImGui::SetCursorScreenPos(ViewButtonPos);

		// 작고 둥근 버튼 스타일
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

		if (ImGui::Button("뷰"))
		{
			ImGui::OpenPopup("ViewOptionsPopup");
		}

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4);

		// 팝업 메뉴 스타일링
		if (ImGui::BeginPopup("ViewOptionsPopup"))
		{
			URenderSettings& Settings = State->World->GetRenderSettings();

			// 언리얼 스타일 팝업 색상
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.70f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

			// 뷰 오버레이 서브메뉴
			if (ImGui::BeginMenu("뷰 오버레이"))
			{
				// 개별 통계 항목 토글
				if (ImGui::MenuItem("파티클 수", nullptr, bShowParticleCount))
				{
					bShowParticleCount = !bShowParticleCount;
				}
				if (ImGui::MenuItem("이벤트 수", nullptr, bShowEventCount))
				{
					bShowEventCount = !bShowEventCount;
				}
				if (ImGui::MenuItem("시간", nullptr, bShowTime))
				{
					bShowTime = !bShowTime;
				}
				if (ImGui::MenuItem("메모리", nullptr, bShowMemory))
				{
					bShowMemory = !bShowMemory;
				}

				ImGui::Separator();

				// 전체 토글
				if (ImGui::MenuItem("모두 표시"))
				{
					bShowParticleCount = true;
					bShowEventCount = true;
					bShowTime = true;
					bShowMemory = true;
				}
				if (ImGui::MenuItem("모두 숨기기"))
				{
					bShowParticleCount = false;
					bShowEventCount = false;
					bShowTime = false;
					bShowMemory = false;
				}

				ImGui::EndMenu();
			}

			// 뷰 모드 서브메뉴
			if (ImGui::BeginMenu("뷰 모드"))
			{
				EViewMode CurrentMode = State->Client->GetViewMode();

				if (ImGui::MenuItem("Unlit", nullptr, CurrentMode == EViewMode::VMI_Unlit))
					State->Client->SetViewMode(EViewMode::VMI_Unlit);
				if (ImGui::MenuItem("Wireframe", nullptr, CurrentMode == EViewMode::VMI_Wireframe))
					State->Client->SetViewMode(EViewMode::VMI_Wireframe);
				if (ImGui::MenuItem("Lit", nullptr, CurrentMode == EViewMode::VMI_Lit_Phong))
					State->Client->SetViewMode(EViewMode::VMI_Lit_Phong);

				ImGui::EndMenu();
			}

			// 그리드 토글
			bool bGrid = Settings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
			if (ImGui::MenuItem("그리드", nullptr, bGrid))
			{
				Settings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
			}

			ImGui::PopStyleVar(1);
			ImGui::PopStyleColor(4);
			ImGui::EndPopup();
		}

		// === "시간" 버튼 (뷰 버튼 오른쪽) ===
		ImVec2 TimeButtonPos(Pos.x + 50.0f, Pos.y + 10.0f);
		ImGui::SetCursorScreenPos(TimeButtonPos);

		// 동일한 버튼 스타일 적용
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

		if (ImGui::Button("시간"))
		{
			ImGui::OpenPopup("TimeOptionsPopup");
		}

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4);

		if (ImGui::BeginPopup("TimeOptionsPopup"))
		{
			// 언리얼 스타일 팝업 색상
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.70f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

			// 재생/일시정지 토글
			const char* PlayPauseLabel = State->bIsSimulating ? "일시정지" : "재생";
			if (ImGui::MenuItem(PlayPauseLabel, nullptr, false))
			{
				State->bIsSimulating = !State->bIsSimulating;
			}

			// 루프 토글
			if (ImGui::MenuItem("루프", nullptr, bIsLooping))
			{
				bIsLooping = !bIsLooping;
			}

			// 애님 스피드 서브메뉴
			if (ImGui::BeginMenu("애님 스피드"))
			{
				float CurrentSpeed = State->SimulationSpeed;

				if (ImGui::MenuItem("100%", nullptr, CurrentSpeed == 1.0f))
					State->SimulationSpeed = 1.0f;
				if (ImGui::MenuItem("50%", nullptr, CurrentSpeed == 0.5f))
					State->SimulationSpeed = 0.5f;
				if (ImGui::MenuItem("25%", nullptr, CurrentSpeed == 0.25f))
					State->SimulationSpeed = 0.25f;
				if (ImGui::MenuItem("10%", nullptr, CurrentSpeed == 0.1f))
					State->SimulationSpeed = 0.1f;
				if (ImGui::MenuItem("1%", nullptr, CurrentSpeed == 0.01f))
					State->SimulationSpeed = 0.01f;

				ImGui::EndMenu();
			}

			ImGui::PopStyleVar(1);
			ImGui::PopStyleColor(4);
			ImGui::EndPopup();
		}

		// === 우하단: 통계 오버레이 ===
		// 하나라도 토글이 켜져 있으면 표시
		bool bAnyStatEnabled = bShowParticleCount || bShowEventCount || bShowTime || bShowMemory;
		if (bAnyStatEnabled && State->PreviewComponent)
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();

			// 통계 수집
			int32 TotalParticles = 0;
			int32 EventCount = 0;
			float MaxEmitterTime = 0.0f;

			for (FParticleEmitterInstance* Instance : State->PreviewComponent->EmitterInstances)
			{
				if (Instance)
				{
					TotalParticles += Instance->ActiveParticles;
					MaxEmitterTime = FMath::Max(MaxEmitterTime, Instance->EmitterTime);
				}
			}

			EventCount = State->PreviewComponent->CollisionEvents.Num()
				+ State->PreviewComponent->SpawnEvents.Num()
				+ State->PreviewComponent->DeathEvents.Num();

			uint64 MemoryBytes = FParticleStatManager::GetInstance().GetStats().MemoryBytes;

			// 개별 토글에 따라 텍스트 생성
			char StatsText[512] = "";
			char TempBuffer[128];

			if (bShowParticleCount)
			{
				sprintf_s(TempBuffer, "파티클 수: %d\n", TotalParticles);
				strcat_s(StatsText, TempBuffer);
			}
			if (bShowEventCount)
			{
				sprintf_s(TempBuffer, "이벤트 수: %d\n", EventCount);
				strcat_s(StatsText, TempBuffer);
			}
			if (bShowTime)
			{
				sprintf_s(TempBuffer, "시간: %.2fs\n", MaxEmitterTime);
				strcat_s(StatsText, TempBuffer);
			}
			if (bShowMemory)
			{
				sprintf_s(TempBuffer, "메모리: %.2f KB\n", MemoryBytes / 1024.0f);
				strcat_s(StatsText, TempBuffer);
			}

			// 마지막 개행 제거
			size_t Len = strlen(StatsText);
			if (Len > 0 && StatsText[Len - 1] == '\n')
			{
				StatsText[Len - 1] = '\0';
			}

			// 우하단 위치
			ImVec2 TextSize = ImGui::CalcTextSize(StatsText);
			ImVec2 TextPos(Pos.x + width - TextSize.x - 10.0f, Pos.y + height - TextSize.y - 10.0f);

			// 텍스트만 표시 (밝은 회색)
			DrawList->AddText(TextPos, IM_COL32(220, 220, 220, 255), StatsText);
		}

		// 3D 축 위젯 (뷰포트 좌측 하단)
		if (State->Client)
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();

			// 축 위젯 설정
			const float AxisSize = 40.0f;
			const float Margin = 15.0f;
			ImVec2 AxisCenter(Pos.x + Margin + AxisSize, Pos.y + height - Margin - AxisSize);

			// 카메라 뷰 행렬에서 회전 부분으로 축 방향 변환
			FMatrix ViewMatrix = State->Client->GetViewMatrix();

			// 3D 축 벡터를 뷰 공간으로 변환 (위치 무시, 방향만)
			FVector AxisX = ViewMatrix.TransformVector(FVector(1, 0, 0));
			FVector AxisY = ViewMatrix.TransformVector(FVector(0, 1, 0));
			FVector AxisZ = ViewMatrix.TransformVector(FVector(0, 0, 1));

			// 축 정보를 배열로 정리 (깊이 정렬용)
			struct AxisInfo
			{
				FVector Dir;
				ImU32 Color;
				const char* Label;
				float Depth;
			};

			// 뷰 공간: X=오른쪽, Y=위쪽, Z=깊이(카메라 방향)
			AxisInfo Axes[3] = {
				{AxisX, IM_COL32(204, 57, 57, 255), "X", AxisX.Z},  // 빨강
				{AxisY, IM_COL32(57, 204, 57, 255), "Y", AxisY.Z},  // 초록
				{AxisZ, IM_COL32(57, 127, 204, 255), "Z", AxisZ.Z}  // 파랑
			};

			// 깊이순 정렬 (뒤→앞, 뒤에 있는 것 먼저 그리기)
			for (int i = 0; i < 2; i++)
			{
				for (int j = i + 1; j < 3; j++)
				{
					if (Axes[i].Depth > Axes[j].Depth)
					{
						AxisInfo Temp = Axes[i];
						Axes[i] = Axes[j];
						Axes[j] = Temp;
					}
				}
			}

			// 정렬된 순서로 그리기
			for (int i = 0; i < 3; i++)
			{
				const AxisInfo& Axis = Axes[i];

				// 2D 화면 좌표로 변환 (X=오른쪽, Y=위쪽)
				// ImGui 화면 Y는 아래가 양수이므로 Y축 반전
				ImVec2 EndPoint(
					AxisCenter.x + Axis.Dir.X * AxisSize,
					AxisCenter.y - Axis.Dir.Y * AxisSize
				);

				// 축 선 그리기
				DrawList->AddLine(AxisCenter, EndPoint, Axis.Color, 2.0f);

				// 라벨 위치 (끝점 방향으로 일정하게 오프셋)
				float DirX = EndPoint.x - AxisCenter.x;
				float DirY = EndPoint.y - AxisCenter.y;
				float Len = sqrtf(DirX * DirX + DirY * DirY);
				if (Len > 0.1f)
				{
					DirX /= Len;
					DirY /= Len;
				}
				ImVec2 LabelPos(EndPoint.x + DirX * 4.0f - 3.0f, EndPoint.y + DirY * 4.0f - 5.0f);
				DrawList->AddText(LabelPos, Axis.Color, Axis.Label);
			}

			// 중심점 원
			DrawList->AddCircleFilled(AxisCenter, 3.0f, IM_COL32(200, 200, 200, 255));
		}

		// Hover 상태 업데이트 (ImGui::Image() 직후에 저장한 값 사용)
		State->Viewport->SetViewportHovered(bViewportHovered);
	}
}

void SParticleEditorWindow::SaveParticleSystem()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystem: 활성 상태가 없습니다");
		return;
	}

	if (!State->EditingTemplate)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystem: 편집 중인 템플릿이 없습니다");
		return;
	}

	// 파일 경로가 없으면 SaveAs 호출
	if (State->CurrentFilePath.empty())
	{
		SaveParticleSystemAs();
		return;
	}

	// 파일로 저장
	if (ParticleEditorBootstrap::SaveParticleSystem(State->EditingTemplate, State->CurrentFilePath))
	{
		// 저장 성공 - Dirty 플래그 해제
		State->bIsDirty = false;

		// ResourceManager에 등록/갱신 (캐시와 동기화)
		UResourceManager::GetInstance().AddOrReplace<UParticleSystem>(State->CurrentFilePath, State->EditingTemplate);

		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 완료: %s", State->CurrentFilePath.c_str());
	}
	else
	{
		// 저장 실패
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 실패: %s", State->CurrentFilePath.c_str());
	}
}

void SParticleEditorWindow::SaveParticleSystemAs()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 활성 상태가 없습니다");
		return;
	}

	if (!State->EditingTemplate)
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 편집 중인 템플릿이 없습니다");
		return;
	}

	// 저장 파일 다이얼로그 열기
	std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
		L"Data/Particles",       // 기본 디렉토리
		L"particle",             // 기본 확장자
		L"Particle Files",       // 파일 타입 설명
		L"NewParticleSystem"     // 기본 파일명
	);

	// 사용자가 취소한 경우
	if (SavePath.empty())
	{
		UE_LOG("[SParticleEditorWindow] SaveParticleSystemAs: 사용자가 취소했습니다");
		return;
	}

	// std::filesystem::path를 FString으로 변환 (UTF-16 → UTF-8, 상대 경로로 변환)
	FString SavePathStr = ResolveAssetRelativePath(NormalizePath(WideToUTF8(SavePath.wstring())), "");

	// 파일로 저장
	if (ParticleEditorBootstrap::SaveParticleSystem(State->EditingTemplate, SavePathStr))
	{
		// SaveAs는 새로운 인스턴스를 생성해야 함 (기존 파일과 분리)
		UParticleSystem* NewTemplate = State->EditingTemplate->Duplicate();
		NewTemplate->SetFilePath(SavePathStr);

		// ResourceManager에 새 인스턴스 등록
		UResourceManager::GetInstance().AddOrReplace<UParticleSystem>(SavePathStr, NewTemplate);

		// 에디터 상태를 새 인스턴스로 교체
		State->EditingTemplate = NewTemplate;
		State->CurrentFilePath = SavePathStr;
		State->bIsDirty = false;

		// PreviewComponent도 새 인스턴스로 교체
		if (State->PreviewComponent)
		{
			State->PreviewComponent->SetTemplate(NewTemplate);
		}

		// 파티클 시스템 캐시 갱신 (새 파일이 Template 선택 목록에 반영되도록)
		UPropertyRenderer::ClearResourcesCache();

		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 완료: %s", SavePathStr.c_str());
	}
	else
	{
		// 저장 실패
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 저장 실패: %s", SavePathStr.c_str());
	}
}

void SParticleEditorWindow::LoadParticleSystem()
{
	ParticleEditorState* State = GetActiveParticleState();
	if (!State)
	{
		UE_LOG("[SParticleEditorWindow] LoadParticleSystem: 활성 상태가 없습니다");
		return;
	}

	// 불러오기 파일 다이얼로그 열기
	std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
		L"Data/Particles",   // 기본 디렉토리
		L"particle",         // 확장자 필터
		L"Particle Files"    // 파일 타입 설명
	);

	// 사용자가 취소한 경우
	if (LoadPath.empty())
	{
		UE_LOG("[SParticleEditorWindow] LoadParticleSystem: 사용자가 취소했습니다");
		return;
	}

	// std::filesystem::path를 FString으로 변환 (UTF-16 → UTF-8)
	FString LoadPathStr = WideToUTF8(LoadPath.wstring());

	// 파일에서 파티클 시스템 로드
	UParticleSystem* LoadedSystem = ParticleEditorBootstrap::LoadParticleSystem(LoadPathStr);
	if (!LoadedSystem)
	{
		UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 실패: %s", LoadPathStr.c_str());
		return;
	}

	// 기존 템플릿 교체
	State->EditingTemplate = LoadedSystem;

	// PreviewComponent에 새 템플릿 설정
	if (State->PreviewComponent)
	{
		State->PreviewComponent->SetTemplate(LoadedSystem);
	}

	// 파일 경로 설정 및 Dirty 플래그 해제
	State->CurrentFilePath = LoadPathStr;
	State->bIsDirty = false;

	// 선택 상태 초기화
	State->SelectedEmitterIndex = -1;
	State->SelectedModuleIndex = -1;
	State->SelectedModule = nullptr;

	UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 완료: %s", LoadPathStr.c_str());
}

void SParticleEditorWindow::RenderLeftViewportArea(float totalHeight)
{
	// 좌측 상단 높이 제한
	LeftViewportHeight = ImClamp(LeftViewportHeight, 100.f, totalHeight - 100.f);

	// 좌측 상단: 뷰포트
	ImGui::BeginChild("ViewportArea", ImVec2(0, LeftViewportHeight), true);
	{
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		RenderViewportArea(viewportSize.x, viewportSize.y);
	}
	ImGui::EndChild();

	// 수평 스플리터 (뷰포트 - 디테일 간)
	ImGui::Button("##LeftHSplitter", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		LeftViewportHeight += delta;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SParticleEditorWindow::RenderLeftDetailsArea()
{
	// 좌측 하단: 디테일 패널
	ImGui::BeginChild("DetailsPanel", ImVec2(0, 0), true);
	{
		float leftWidth = ImGui::GetContentRegionAvail().x;
		RenderDetailsPanel(leftWidth);
	}
	ImGui::EndChild();
}

void SParticleEditorWindow::RenderRightEmitterArea(float totalHeight)
{
	// 우측 상단 높이 제한
	RightEmitterHeight = ImClamp(RightEmitterHeight, 100.f, totalHeight - 100.f);

	// 우측 상단: 이미터 패널
	ImGui::BeginChild("EmittersPanel", ImVec2(0, RightEmitterHeight), true);
	{
		RenderRightPanel();
	}
	ImGui::EndChild();

	// 수평 스플리터 (이미터 - 커브 간)
	ImGui::Button("##RightHSplitter", ImVec2(-1, 4.f));
	if (ImGui::IsItemActive())
	{
		float delta = ImGui::GetIO().MouseDelta.y;
		RightEmitterHeight += delta;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
}

void SParticleEditorWindow::RenderRightCurveArea()
{
	// 우측 하단: 커브 에디터
	ImGui::BeginChild("CurveEditorPanel", ImVec2(0, 0), true);
	{
		RenderBottomPanel();
	}
	ImGui::EndChild();
}

void SParticleEditorWindow::OpenOrFocusTab(UEditorAssetPreviewContext* Context)
{
	// Context가 없거나 AssetPath가 비어있으면 새 탭 열기
	if (!Context || Context->AssetPath.empty())
	{
		char label[64];
		sprintf_s(label, "Particle Editor %d", Tabs.Num() + 1);
		OpenNewTab(label);
		return;
	}

	// 기존 탭 중 동일한 파일 경로를 가진 탭 검색
	for (int i = 0; i < Tabs.Num(); ++i)
	{
		ParticleEditorState* State = static_cast<ParticleEditorState*>(Tabs[i]);
		if (State && State->CurrentFilePath == Context->AssetPath)
		{
			// 기존 탭으로 전환
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			UE_LOG("[SParticleEditorWindow] 기존 탭 발견: %s", Context->AssetPath.c_str());
			return;
		}
	}

	// 기존 탭이 없으면 새 탭 생성
	UE_LOG("[SParticleEditorWindow] 새 탭 생성: %s", Context->AssetPath.c_str());

	// 파일 경로에서 파일 이름 추출
	FString AssetName;
	const size_t lastSlash = Context->AssetPath.find_last_of("/\\");
	if (lastSlash != FString::npos)
		AssetName = Context->AssetPath.substr(lastSlash + 1);
	else
		AssetName = Context->AssetPath;

	// 확장자 제거
	const size_t dotPos = AssetName.find_last_of('.');
	if (dotPos != FString::npos)
		AssetName = AssetName.substr(0, dotPos);
	ViewerState* NewState = CreateViewerState(AssetName.c_str(), Context);

	if (NewState)
	{
		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;

		// 파티클 시스템 로드
		ParticleEditorState* ParticleState = static_cast<ParticleEditorState*>(NewState);
		UParticleSystem* LoadedSystem = ParticleEditorBootstrap::LoadParticleSystem(Context->AssetPath);
		if (LoadedSystem)
		{
			ParticleState->EditingTemplate = LoadedSystem;
			ParticleState->CurrentFilePath = LoadedSystem->GetFilePath(); // 정규화된 경로 사용
			ParticleState->bIsDirty = false;

			// PreviewComponent에 새 템플릿 설정
			if (ParticleState->PreviewComponent)
			{
				ParticleState->PreviewComponent->SetTemplate(LoadedSystem);
			}

			UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 완료: %s", Context->AssetPath.c_str());
		}
		else
		{
			UE_LOG("[SParticleEditorWindow] 파티클 시스템 로드 실패: %s", Context->AssetPath.c_str());
		}
	}
}

// ============================================================
// 커브 에디터 함수들
// ============================================================

bool SParticleEditorWindow::HasCurveProperties(UParticleModule* Module)
{
	// 커브 에디터 위젯에 위임
	if (CurveEditorWidget)
	{
		return CurveEditorWidget->HasCurveProperties(Module);
	}
	return false;
}

void SParticleEditorWindow::ToggleCurveTrack(UParticleModule* Module)
{
	// 커브 에디터 위젯에 위임
	if (CurveEditorWidget)
	{
		CurveEditorWidget->ToggleModuleTracks(Module);
	}
}
