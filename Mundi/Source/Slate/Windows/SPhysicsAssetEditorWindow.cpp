#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "FViewport.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/PhysGroundActor.h"
#include "Source/Runtime/Engine/Components/BoxComponent.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "PhysicsUtils.h"
#include "PlatformProcess.h"

static EBodySetupType ToBodySetupType(EPrimitiveType InPrimitiveType)
{
    switch (InPrimitiveType)
    {
    case EPrimitiveType::Sphere:    return EBodySetupType::Sphere;
    case EPrimitiveType::Box:       return EBodySetupType::Box;
    case EPrimitiveType::Capsule:   return EBodySetupType::Capsule;
    default:                        return EBodySetupType::None;
    }
}

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    // 시뮬레이션 중이면 먼저 정지 (물리 리소스 정리)
    if (bIsSimulating)
    {
        StopSimulation();
    }

    // Cleanup
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        PhysicsAssetEditorBootstrap::DestroyViewerState(State);  // 또는 전용 Bootstrap
    }
    Tabs.Empty();
    ActiveState = nullptr;

    if (IconSingleBody)
    {
        DeleteObject(IconSingleBody);
        IconSingleBody = nullptr;
    }
    if (IconMultipleBody)
    {
        DeleteObject(IconMultipleBody);
        IconMultipleBody = nullptr;
    }
    if (IconBoneConstraint)
    {
        DeleteObject(IconBoneConstraint);
        IconBoneConstraint = nullptr;
    }
    if (IconBoneCrossConstraint)
    {
        DeleteObject(IconBoneCrossConstraint);
        IconBoneConstraint = nullptr;
    }
    if (IconPlay)
    {
        DeleteObject(IconPlay);
        IconPlay = nullptr;
    }
    if (IconPause)
    {
        DeleteObject(IconPause);
        IconPause = nullptr;
    }
}

void SPhysicsAssetEditorWindow::OnRender()
{
    // If window is closed, request cleanup and don't render
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }
    
    if (!bIconsLoaded && Device)
    {
        IconSingleBody = NewObject<UTexture>();
        IconSingleBody->Load(GDataDir + "/Icon/SingleBody.png", Device);

        IconMultipleBody = NewObject<UTexture>();
        IconMultipleBody->Load(GDataDir + "/Icon/MultipleBody.png", Device);

        IconBoneConstraint = NewObject<UTexture>();
        IconBoneConstraint->Load(GDataDir + "/Icon/BoneConstraint.png", Device);

        IconBoneCrossConstraint = NewObject<UTexture>();
        IconBoneCrossConstraint->Load(GDataDir + "/Icon/BoneCrossConstraint.png", Device);

        IconPlay = NewObject<UTexture>();
        IconPlay->Load(GDataDir + "/Icon/Anim_Play.png", Device);

        IconPause = NewObject<UTexture>();
        IconPause->Load(GDataDir + "/Icon/Anim_Pause.png", Device);

        bIconsLoaded = true;
    }

    // Parent detachable window (movable, top-level) with solid background
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }
    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }

    // Generate a unique window title to pass to ImGui
    // Format: "Skeletal Mesh Viewer - MyAsset.fbx###0x12345678"
    char UniqueTitle[256];
    FString Title = GetWindowTitle();
    if (Tabs.Num() == 1 && ActiveState && !ActiveState->LoadedMeshPath.empty())
    {
        std::filesystem::path fsPath(UTF8ToWide(ActiveState->LoadedMeshPath));
        FString AssetName = WideToUTF8(fsPath.filename().wstring());
        Title += " - " + AssetName;
    }
    sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

    bool bViewerVisible = false;
    if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
    {
        bViewerVisible = true;

        // 입력 라우팅을 위한 hover/focus 상태 캡처
        bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Render tab bar and switch active state
        /*if (!ImGui::BeginTabBar("SkeletalViewerTabs",
            ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
            return;*/
        RenderTabsAndToolbar(EViewerType::PhysicsAsset);

        // 마지막 탭을 닫은 경우 렌더링 중단
        if (!bIsOpen)
        {
            USlateManager::GetInstance().RequestCloseDetachedWindow(this);
            ImGui::End();
            return;
        }

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float splitterWidth = 4.0f; // 분할선 두께

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);

        // 중앙 패널이 음수가 되지 않도록 보정 (안전장치)
        if (centerWidth < 0.0f)
        {
            centerWidth = 0.0f;
        }

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel - Asset Browser & Bone Hierarchy
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        RenderLeftPanel(leftWidth);
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Left splitter (드래그 가능한 분할선)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newLeftRatio = LeftPanelRatio + delta / totalWidth;
                // 좌측 패널 최소 10%, 우측 패널과 겹치지 않도록 제한
                float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
                LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
            }
        }

        ImGui::SameLine(0, 0); // No spacing between panels

        // Center panel (viewport area) - 완전히 가려진 경우 렌더링하지 않음
        if (centerWidth > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
            ImGui::PopStyleVar();

            // 뷰어 툴바 렌더링 (뷰포트 상단)
            RenderViewerToolbar();

            // 툴바 아래 뷰포트 영역
            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            float remainingWidth = ImGui::GetContentRegionAvail().x;
            float remainingHeight = ImGui::GetContentRegionAvail().y;

            // 뷰포트 영역 설정
            CenterRect.Left = viewportPos.x;
            CenterRect.Top = viewportPos.y;
            CenterRect.Right = viewportPos.x + remainingWidth;
            CenterRect.Bottom = viewportPos.y + remainingHeight;
            CenterRect.UpdateMinMax();

            // 뷰포트 렌더링 (텍스처에)
            OnRenderViewport();

            // ImGui::Image로 결과 텍스처 표시
            if (ActiveState && ActiveState->Viewport)
            {
                ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
                if (SRV)
                {
                    ImGui::Image((void*)SRV, ImVec2(remainingWidth, remainingHeight));
                    // ImGui의 Z-order를 고려한 정확한 hover 체크
                    ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
                }
                else
                {
                    ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
                    ActiveState->Viewport->SetViewportHovered(false);
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(remainingWidth, remainingHeight));
            }

            ImGui::EndChild(); // CenterPanel

            ImGui::SameLine(0, 0); // No spacing between panels
        }
        else
        {
            // 중앙 패널이 완전히 가려진 경우 뷰포트 영역 초기화
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        // Right splitter (드래그 가능한 분할선)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newRightRatio = RightPanelRatio - delta / totalWidth;
                // 우측 패널 최소 10%, 좌측 패널과 겹치지 않도록 제한
                float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
                RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
            }
        }

        ImGui::SameLine(0, 0); // No spacing between panels

        // Right panel - Bone Properties & Tools
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        {
            float propertiesHeight = ImGui::GetContentRegionAvail().y * 0.6f;

            ImGui::BeginChild("BonePropertiesArea", ImVec2(0, propertiesHeight), false);
            RenderRightPanel();
            ImGui::EndChild();
            
            ImGui::BeginChild("ToolsArea", ImVec2(0, 0), false);
            RenderToolsPanel();
            ImGui::EndChild();
        }
        ImGui::EndChild(); // RightPanel
        ImGui::PopStyleVar();   // Pop the ItemSpacing style
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        // Just request to close the window
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
    }

    bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    // 기본 포즈 위에 사용자가 조작한 본 트랜스폼 오프셋을 적용
    // SAnimationViewerWindow는 TickComponent()가 매 프레임 포즈를 리셋하지만,
    // 여기서는 애니메이션이 없으므로 수동으로 RefPose로 리셋해야 누적을 방지할 수 있음
    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // 시뮬레이션 모드일 때: 물리 결과를 본 트랜스폼에 동기화
        if (bIsSimulating)
        {
            // 물리 시뮬레이션 결과로 본 트랜스폼 업데이트
            // (SkeletalMeshComponent::TickComponent에서 이미 호출되지만,
            //  PreviewWorld에서 확실히 동작하도록 수동으로 호출)
            MeshComp->UpdateBoneTransformsFromPhysics();

            // 시뮬레이션 중에도 본 라인 시각화 갱신
            if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh)
            {
                if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    LineComp->SetLineVisible(true);
                }
                ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
            }

            // 시뮬레이션 중에도 충돌 형상 시각화 갱신
            bCollisionShapesDirty = true;
        }
        else
        {
            // 시뮬레이션 모드가 아닐 때만 수동 조작 및 포즈 리셋을 처리
            // 누적 방지를 위해 먼저 참조 포즈로 리셋
            MeshComp->ResetToRefPose();

            if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
            {
                MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
            }

            // 본이 선택된 경우, 기즈모 위치를 본의 최종 트랜스폼에 맞춰 업데이트
            if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->World)
            {
                AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
                bool bCurrentlyDragging = Gizmo && Gizmo->GetbIsDragging();

                // 드래그 첫 프레임인지 확인 (World→Relative→World 변환 오차 방지)
                bool bIsFirstDragFrame = bCurrentlyDragging && !ActiveState->bWasGizmoDragging;

                if (bCurrentlyDragging && !bIsFirstDragFrame)
                {
                    // 첫 프레임이 아닐 때만 기즈모로부터 트랜스폼 업데이트
                    UpdateBoneTransformFromGizmo(ActiveState);
                    // Mark collision shapes dirty to update in real-time
                    bCollisionShapesDirty = true;
                }
                else if (!bCurrentlyDragging)
                {
                    ActiveState->PreviewActor->RepositionAnchorToBone(ActiveState->SelectedBoneIndex);
                }
                // 첫 프레임에서는 아무것도 하지 않음 (앵커가 아직 움직이지 않았으므로)

                // 드래그 상태 업데이트 (다음 프레임에서 첫 프레임 감지용)
                ActiveState->bWasGizmoDragging = bCurrentlyDragging;
            }

            // Reconstruct bone overlay
            if (ActiveState->bShowBones)
            {
                ActiveState->bBoneLinesDirty = true;
            }
            if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
            {
                if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    LineComp->SetLineVisible(true);
                }
                ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
                ActiveState->bBoneLinesDirty = false;
            }
        }
    }

    // Rebuild collision shapes if dirty
    if (bCollisionShapesDirty && bShowCollision)
    {
        RebuildCollisionShapes();
    }
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    ViewerState* NewState = PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device);
    if (!NewState) return nullptr;

    if (Context && !Context->AssetPath.empty())
    {
        LoadSkeletalMesh(NewState, Context->AssetPath);
    }
    return NewState;
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
    PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::RenderContextualControls()
{
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2);
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Center buttons vertically
    const float ToolbarHeight = 30.0f;  // Hard Coded !!!!
    float textHeight = ImGui::CalcTextSize("Simulate").y;
    float BtnHeight = textHeight + style.FramePadding.y * 2.0f;
    float YOffset = (ToolbarHeight - BtnHeight) * 0.5f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + YOffset);

    // Calculate a fixed width based on the "Simulate" text.
    float textWidth = ImGui::CalcTextSize("Simulate").x;
    ImVec2 textButtonSize(textWidth + style.FramePadding.x * 2, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));

    // Simulate Button
    if (ImGui::Button(bIsSimulating ? "Stop" : "Simulate", textButtonSize))
    {
        bIsSimulating = !bIsSimulating;
        if (bIsSimulating)
        {
            StartSimulation();
        }
        else
        {
            StopSimulation();
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 0);
}

void SPhysicsAssetEditorWindow::RenderHierarchySection()
{
    RenderPhysicsBodyHierarchy();
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
    if (!ActiveState)   return;

    if (ActiveState->SelectedBodyIndex >= 0)
    {
        RenderBodyProperties();
    }
    else if (ActiveState->SelectedConstraintIndex >= 0)
    {
        RenderConstraintProperties();
    }
    else
    {
        SViewerWindow::RenderRightPanel();
    }
}

void SPhysicsAssetEditorWindow::RenderPhysicsBodyHierarchy()
{
    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::TextWrapped("No skeletal mesh loaded.");
        ImGui::Spacing();
        ImGui::TextDisabled("Load a skeletal mesh from the Asset Browser to begin.");
        return;
    }

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    if (!PhysicsAsset) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.IsEmpty())
    {
        ImGui::TextWrapped("This mesh has no skeleton data.");
        return;
    }

    const TArray<FBone>& Bones = Skeleton->Bones;

    // Build parent-child adjacency list
    TArray<TArray<int32>> Children;
    Children.resize(Bones.size());
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        int32 Parent = Bones[i].ParentIndex;
        if (Parent >= 0 && Parent < Bones.size())
        {
            Children[Parent].Add(i);
        }
    }

    ImGui::BeginChild("PhysicsBodyTreeView", ImVec2(0, 0), true);

    // Recursive drawing function
    std::function<void(int32)> DrawNode = [&](int32 BoneIndex)
    {
        const FBone& Bone = Bones[BoneIndex];
        FName BoneName(Bone.Name);

        // Check if this bone has a physics body
        int32 BodyIndex = -1;
        bool bHasBody = false;
        if (ActiveState->CurrentPhysicsAsset)
        {
            BodyIndex = ActiveState->CurrentPhysicsAsset->FindBodyIndex(BoneName);
            bHasBody = (BodyIndex >= 0);
        }

        // A bone is a leaf if it has no child bones AND no body to display.
        const bool bIsTreeLeaf = Children[BoneIndex].IsEmpty() && !bHasBody;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
        if (bIsTreeLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (ActiveState->ExpandedBoneIndices.count(BoneIndex) > 0)
        {
            ImGui::SetNextItemOpen(true);
        }
        
        bool bIsBoneSelected = (ActiveState->SelectedBoneIndex == BoneIndex && ActiveState->SelectedBodyIndex < 0);
        if (bIsBoneSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
        }

        // Render the BONE tree node
        bool open = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, flags, "%s", Bone.Name.c_str());

        if (bIsBoneSelected)
        {
            ImGui::PopStyleColor(3);
        }

        // Context menu for the BONE
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            ActiveState->SelectedBoneIndex = BoneIndex;
            ActiveState->SelectedBodyIndex = -1;
            ActiveState->SelectedConstraintIndex = -1;
            bCollisionShapesDirty = true;
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (!bHasBody && ImGui::MenuItem("Create Body")) { CreateBodyForBone(BoneIndex, SelectedPrimitiveType); }
            ImGui::EndPopup();
        }
        
        if (ImGui::IsItemToggledOpen())
        {
            if (open) ActiveState->ExpandedBoneIndices.insert(BoneIndex);
            else ActiveState->ExpandedBoneIndices.erase(BoneIndex);
        }

        if (open)
        {
            // 1. Render Body node if it exists
            if (bHasBody)
            {
                UBodySetup* BodySetup = ActiveState->CurrentPhysicsAsset->GetBodySetups()[BodyIndex];
                if (BodySetup)
                {
                    // Determine body label
                    FString BodyLabel = "Aggregate Body";
                    int primitiveCount = BodySetup->AggGeom.GetElementCount();
                    if (primitiveCount == 1)
                    {
                        if (BodySetup->AggGeom.SphereElems.Num() == 1) BodyLabel = "Sphere";
                        else if (BodySetup->AggGeom.BoxElems.Num() == 1) BodyLabel = "Box";
                        else if (BodySetup->AggGeom.SphylElems.Num() == 1) BodyLabel = "Capsule";
                        else if (BodySetup->AggGeom.ConvexElems.Num() == 1) BodyLabel = "Convex";
                    }

                    ImGuiTreeNodeFlags bodyFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                        ImGuiTreeNodeFlags_SpanFullWidth;
                    
                    bool bIsBodySelected = (ActiveState->SelectedBodyIndex == BodyIndex);
                    if (bIsBodySelected) {
                        bodyFlags |= ImGuiTreeNodeFlags_Selected;
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.55f, 0.55f, 0.55f, 0.7f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f)); // Gold text
                    }

                    if (IconSingleBody)
                    {
                        float iconSize = ImGui::GetTextLineHeight();
                        ImGui::Image((void*)IconSingleBody->GetShaderResourceView(), ImVec2(iconSize, iconSize));
                        ImGui::SameLine(0.0f, 0.0f);
                    }

                    bool isBodyNodeOpen = ImGui::TreeNodeEx((void*)BodySetup, bodyFlags, "%s [Body]", BodyLabel.c_str());

                    if(bIsBodySelected) ImGui::PopStyleColor(3);
                    else ImGui::PopStyleColor();

                    if (ImGui::IsItemClicked()) {
                        ActiveState->SelectedBodyIndex = BodyIndex;
                        ActiveState->SelectedBoneIndex = BoneIndex;
                        ActiveState->SelectedConstraintIndex = -1;
                        bCollisionShapesDirty = true;
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::IsItemClicked()) {
                            ActiveState->SelectedBodyIndex = BodyIndex;
                            ActiveState->SelectedBoneIndex = BoneIndex;
                            ActiveState->SelectedConstraintIndex = -1;
                            bCollisionShapesDirty = true;
                        }
                        if (ImGui::MenuItem("Add Primitive...")) { /* TODO */ }
                        
                        if (ImGui::BeginMenu("Add Constraint"))
                        {
                            UPhysicsAsset* PA = ActiveState->CurrentPhysicsAsset;
                            const TArray<UBodySetup*>& Bodies = PA->GetBodySetups();

                            for (int32 i = 0; i < Bodies.Num(); ++i)
                            {
                                if (i == BodyIndex)
                                    continue; // 자기 자신 제외

                                FString Name = Bodies[i]->BoneName.ToString();

                                if (ImGui::MenuItem(Name.c_str()))
                                {
                                    CreateConstraintBetweenBodies(BodyIndex, i);
                                    ActiveState->SelectedBodyIndex = BodyIndex;
                                    bCollisionShapesDirty = true;
                                }
                            }
                            ImGui::EndMenu();
                        }
                        ImGui::EndPopup();
                    }
                }
            }

            // 2. Render constraints connected to this bone
            if (ActiveState->CurrentPhysicsAsset)
            {
                const TArray<FConstraintSetup>& Constraints =
                    ActiveState->CurrentPhysicsAsset->GetContraintSetups();

                for (int32 i = 0; i < Constraints.Num(); ++i)
                {
                    const FConstraintSetup& Setup = Constraints[i];

                    if (Setup.ConstraintBone1 == BoneName || Setup.ConstraintBone2 == BoneName)
                    {
                        // Tree flags
                        ImGuiTreeNodeFlags cFlags =
                            ImGuiTreeNodeFlags_Leaf |
                            ImGuiTreeNodeFlags_NoTreePushOnOpen |
                            ImGuiTreeNodeFlags_SpanFullWidth;

                        FString OtherBone =
                            (Setup.ConstraintBone1 == BoneName)
                            ? Setup.ConstraintBone2.ToString()
                            : Setup.ConstraintBone1.ToString();

                        FString Label = "Constraint to " + OtherBone;

                        // Selected highlight
                        bool bIsConstraintSelected = (ActiveState->SelectedConstraintIndex == i);
                        if (bIsConstraintSelected)
                        {
                            cFlags |= ImGuiTreeNodeFlags_Selected;
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.5f, 0.5f, 0.2f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.2f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.55f, 0.55f, 0.55f, 0.2f));
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f)); // 파란 글씨
                        }

                        if (IconBoneConstraint)
                        {
                            float iconSize = ImGui::GetTextLineHeight();
                            ImGui::Image((void*)IconBoneConstraint->GetShaderResourceView(), ImVec2(iconSize, iconSize));
                            ImGui::SameLine(0.0f, 0.0f);
                        }

                        ImGui::TreeNodeEx((void*)(intptr_t)(&Setup), cFlags, "[%s -> %s] Constraint", 
                            Setup.ConstraintBone1.ToString().c_str(),
                            Setup.ConstraintBone2.ToString().c_str());

                        ImGui::PopStyleColor(bIsConstraintSelected ? 3 : 1);

                        // 클릭 처리
                        if (ImGui::IsItemClicked())
                        {
                            ActiveState->SelectedConstraintIndex = i;
                            ActiveState->SelectedBoneIndex = -1;
                            ActiveState->SelectedBodyIndex = -1;
                            bCollisionShapesDirty = true;
                        }

                        // Context Menu
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Delete Constraint"))
                            {
                                PhysicsAsset->GetConstraintSetupsMutable().RemoveAt(i);
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
            }

            // 3. Render child bones recursively
            for (int32 Child : Children[BoneIndex])
            {
                DrawNode(Child);
            }

            if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            {
                ImGui::TreePop(); // Pop for Bone
            }
        }
    };

    // Draw root bones
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        if (Bones[i].ParentIndex < 0)
        {
            DrawNode(i);
        }
    }

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderToolsPanel()
{
    // Panel Header
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("TOOLS");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Spacing();

    // Body Creation Section
    ImGui::Selectable("Body Creation", false, ImGuiSelectableFlags_Disabled, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 4.0f));
    ImGui::Dummy(ImVec2(0, 4));

    // Select Primitive Type
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.2f, 2.2f));
    ImGui::RadioButton("Sphere", (int*)&SelectedPrimitiveType, (int)EPrimitiveType::Sphere);
    ImGui::SameLine(0, 8.0f);
    ImGui::RadioButton("Box", (int*)&SelectedPrimitiveType, (int)EPrimitiveType::Box);
    ImGui::SameLine(0, 8.0f);
    ImGui::RadioButton("Capsule", (int*)&SelectedPrimitiveType, (int)EPrimitiveType::Capsule);
    ImGui::SameLine(0, 8.0f);
    ImGui::RadioButton("Convex", (int*)&SelectedPrimitiveType, (int)EPrimitiveType::Convex);
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));
    
    // Generation Method Section
    // Checkbox Style
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5, 1.5));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f)); // #3A3F45 계열
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.28f, 0.30f, 0.33f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.22f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

    ImGui::Checkbox("Use bone-length based generation", &bUseBoneLengthGeneration);
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // [Generate All Bodies] button
    // Activated only when a Skeletal Mesh is loaded
    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Generate All Bodies", ImVec2(-1, 30)))
    {
        const char* TypeStr = "";
        switch (SelectedPrimitiveType)
        {
        case EPrimitiveType::Sphere: TypeStr = "Sphere"; break;
        case EPrimitiveType::Box:    TypeStr = "Box";    break;
        case EPrimitiveType::Capsule:TypeStr = "Capsule"; break;
        }
        UE_LOG("GENERATE ALL BODIES: PrimitiveType = %s", TypeStr);

        // Generate physics bodies for all bones
        GenerateAllBodies(SelectedPrimitiveType);

        // Trigger collision shape visualization
        bCollisionShapesDirty = true;

        ActiveState->bIsDirty = true;
    }

    ImGui::Dummy(ImVec2(0, 5));

    // [Clear All Bodies] button
    bool hasAnyBodies = ActiveState && ActiveState->CurrentPhysicsAsset &&
                        ActiveState->CurrentPhysicsAsset->GetBodySetupCount() > 0;
    if (!hasAnyBodies)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Clear All Bodies", ImVec2(-1, 30)))
    {
        if (ActiveState && ActiveState->CurrentPhysicsAsset)
        {
            int32 BodyCount = ActiveState->CurrentPhysicsAsset->GetBodySetupCount();
            UE_LOG("CLEAR ALL BODIES: Removing %d bodies", BodyCount);

            ActiveState->CurrentPhysicsAsset->ClearAllBodies();

            // Reset selection indices to prevent accessing invalid array elements
            ActiveState->SelectedBodyIndex = -1;
            ActiveState->SelectedConstraintIndex = -1;
            ActiveState->SelectedBoneIndex = -1;

            // Clear visualization
            bCollisionShapesDirty = true;
            ActiveState->bIsDirty = true;
        }
    }

    if (!hasAnyBodies)
    {
        ImGui::EndDisabled();
    }

    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::EndDisabled();
    }

    // +-+-+ Constraint Tools Section +-+-+
    ImGui::Dummy(ImVec2(0, 12));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    ImGui::Selectable("Constraint Tools", false, ImGuiSelectableFlags_Disabled,
        ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 4.0f));

    ImGui::Dummy(ImVec2(0, 4));

    // Generate All Constraints 버튼
    bool meshLoaded = ActiveState && ActiveState->CurrentMesh;
    if (!meshLoaded)
        ImGui::BeginDisabled();

    if (ImGui::Button("Generate All Constraints", ImVec2(-1, 30)))
    {
        GenerateAllConstraints();
        ActiveState->bIsDirty = true;
    }

    ImGui::Dummy(ImVec2(0, 5));

    // Clear All Constraints 버튼
    bool hasConstraints =
        ActiveState &&
        ActiveState->CurrentPhysicsAsset &&
        ActiveState->CurrentPhysicsAsset->GetConstraintSetupCount() > 0;

    if (!hasConstraints)
        ImGui::BeginDisabled();

    if (ImGui::Button("Clear All Constraints", ImVec2(-1, 30)))
    {
        UE_LOG("CLEAR ALL CONSTRAINTS");
        ActiveState->CurrentPhysicsAsset->GetConstraintSetupsMutable().Empty();
        ActiveState->bIsDirty = true;
    }

    if (!hasConstraints)
        ImGui::EndDisabled();

    if (!meshLoaded)
        ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, 5));
}

void SPhysicsAssetEditorWindow::OnSave()
{
    if (!ActiveState || !ActiveState->CurrentMesh || !ActiveState->CurrentPhysicsAsset)
    {
        UE_LOG("[SPhysicsAssetEditorWindow/OnSave] 저장할 PhysicsAsset이 없습니다.");
        return;
    }
    /*
     * 어떤 메시의 애셋인지 지정하기 위한 CurrentMesh
     * 피직스애셋 정보 저장
     */
    USkeletalMesh* SkeletalMesh = ActiveState->CurrentMesh;
    if (!SkeletalMesh)
    {
        return;
    }

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    if (!PhysicsAsset)
    {
        return;
    }
    
    std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
        L"Data/PhysicsAsset",       // 기본 디렉토리
        L"PhysicsAsset",             // 기본 확장자
        L"PhysicsAsset Files",       // 파일 타입 설명
        L"NewPhysicsAsset"     // 기본 파일명
    );
    
    if (SavePath.empty())
    {
        UE_LOG("[SPhysicsAssetEditorWindow] OnSave: 사용자가 취소했습니다");
        return;
    }

    FString SavePathStr = ResolveAssetRelativePath(NormalizePath(WideToUTF8(SavePath.wstring())), "");
    PhysicsAsset->SetMeshFilePath(ActiveState->CurrentMesh->GetPathFileName());
    if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(PhysicsAsset, SavePathStr))
    {
        UResourceManager::GetInstance().AddOrReplacePhysicsAsset(SavePathStr, PhysicsAsset);
        ActiveState->bIsDirty = false;
        UE_LOG("[SPhysicsAssetEditorWindow] PhysicsAsset 저장 완료: %s", SavePathStr.c_str());
    }
    else
    {
        UE_LOG("[SPhysicsAssetEditorWindow] PhysicsAsset 저장 실패: %s", SavePathStr.c_str());
    }
    
}

void SPhysicsAssetEditorWindow::OnLoad()
{
    if (!ActiveState)
    {
        return;
    }
    
    std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(
        L"Data/PhysicsAsset",
        L"PhysicsAsset",
        L"PhysicsAsset Files"
    );
    
    if (SelectedPath.empty())
    {
        return;
    }

    FString SelectedPathStr = ResolveAssetRelativePath(NormalizePath(WideToUTF8(SelectedPath.wstring())), "");
    if (UPhysicsAsset* Asset = PhysicsAssetEditorBootstrap::LoadPhysicsAsset(SelectedPathStr, ActiveState->LoadedMeshPath))
    {
        ActiveState->CurrentPhysicsAsset = Asset;
        ActiveState->CurrentMesh->SetPhysicsAsset(Asset);
        if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetPhysicsAsset(Asset);
        }
        
        UE_LOG("[SPhysicsAssetEditorWindow] PhysicsAsset 불러오기 성공: %s", SelectedPathStr.c_str());
        ActiveState->bBoneLinesDirty = true;
        bCollisionShapesDirty = true;
    }
    else
    {
        UE_LOG("[SPhysicsAssetEditorWindow] PhysicsAsset 불러오기 실패: %s", SelectedPathStr.c_str());
    }
}

void SPhysicsAssetEditorWindow::LoadSkeletalMesh(ViewerState* State, const FString& Path)
{
    if (!State || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && State->PreviewActor)
    {
        // Set the mesh on the preview actor
        State->PreviewActor->SetSkeletalMesh(Path);
        State->CurrentMesh = Mesh;

        // SkeletalMesh의 기존 PhysicsAsset을 사용하거나, 없으면 새로 생성하여 연결
        UPhysicsAsset* ExistingPhysicsAsset = Mesh->GetPhysicsAsset();
        if (ExistingPhysicsAsset)
        {
            State->CurrentPhysicsAsset = ExistingPhysicsAsset;
            UE_LOG("SPhysicsAssetEditorWindow: Using existing PhysicsAsset from SkeletalMesh %s", Path.c_str());
        }
        else
        {
            State->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();
            Mesh->SetPhysicsAsset(State->CurrentPhysicsAsset);
            UE_LOG("SPhysicsAssetEditorWindow: Created new PhysicsAsset and linked to SkeletalMesh %s", Path.c_str());
        }

        // Expand all bone nodes by default on mesh load
        State->ExpandedBoneIndices.clear();
        if (const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton())
        {
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                State->ExpandedBoneIndices.insert(i);
            }
        }

        State->LoadedMeshPath = Path;  // Track for resource unloading

        // Update mesh path buffer for display in UI
        strncpy_s(State->MeshPathBuffer, Path.c_str(), sizeof(State->MeshPathBuffer) - 1);

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = State->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(State->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        State->bBoneLinesDirty = true;

        // Clear and sync bone line visibility
        if (auto* LineComp = State->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(State->bShowBones);
        }

        UE_LOG("SPhysicsAssetEditorWindow: Loaded skeletal mesh from %s", Path.c_str());

    }
    else
    {
        UE_LOG("SPhysicsAssetEditorWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

// ImGui draw callback - Direct3D 뷰포트 렌더링
void SPhysicsAssetEditorWindow::ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // UserCallbackData로 전달된 this 포인터 가져오기
    SPhysicsAssetEditorWindow* window = (SPhysicsAssetEditorWindow*)cmd->UserCallbackData;

    if (window && window->ActiveState && window->ActiveState->Viewport)
    {
        FViewport* viewport = window->ActiveState->Viewport;

        // D3D 디바이스 컨텍스트 가져오기
        ID3D11Device* device = window->Device;
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);

        if (context)
        {
            // ImGui가 변경한 D3D 상태를 뷰포트 렌더링에 맞게 초기화
            // 1. Viewport 설정
            D3D11_VIEWPORT d3dViewport = {};
            d3dViewport.Width = static_cast<float>(viewport->GetSizeX());
            d3dViewport.Height = static_cast<float>(viewport->GetSizeY());
            d3dViewport.MinDepth = 0.0f;
            d3dViewport.MaxDepth = 1.0f;
            d3dViewport.TopLeftX = static_cast<float>(viewport->GetStartX());
            d3dViewport.TopLeftY = static_cast<float>(viewport->GetStartY());
            context->RSSetViewports(1, &d3dViewport);

            // 2. D3D 렌더 상태 복구 (ImGui → 3D 렌더링용)
            float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);  // 블렌딩 비활성화
            context->OMSetDepthStencilState(nullptr, 0);                  // 기본 깊이 테스트
            context->RSSetState(nullptr);                                 // 기본 래스터라이저
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // 3. 뷰포트 렌더링 실행
            window->OnRenderViewport();

            context->Release();
        }
    }
}

void SPhysicsAssetEditorWindow::RenderBodyProperties()
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset) return;
    if (ActiveState->SelectedBodyIndex < 0) return;

    // Bounds check before accessing array
    const TArray<UBodySetup*>& BodySetups = ActiveState->CurrentPhysicsAsset->GetBodySetups();
    if (ActiveState->SelectedBodyIndex >= BodySetups.Num())
    {
        // Selection index is out of range, reset it
        ActiveState->SelectedBodyIndex = -1;
        return;
    }

    UBodySetup* SelectedBody = BodySetups[ActiveState->SelectedBodyIndex];
    if (!SelectedBody) return;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("BODY PROPERTIES");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));

    bool bChanged = false;

    // Store previous values to detect changes
    EBodySetupType PrevBodyType = SelectedBody->BodyType;

    // === Body Type Selection ===
    ImGui::Text("Body Type");
    ImGui::Spacing();

    const char* BodyTypeNames[] = { "None", "Box", "Sphere", "Capsule" };
    int CurrentBodyType = (int)SelectedBody->BodyType;

    if (ImGui::Combo("##BodyType", &CurrentBodyType, BodyTypeNames, IM_ARRAYSIZE(BodyTypeNames)))
    {
        SelectedBody->BodyType = (EBodySetupType)CurrentBodyType;
        bChanged = true;

        // BodyType changed - reconstruct primitive
        if (PrevBodyType != SelectedBody->BodyType)
        {
            EPrimitiveType NewPrimitiveType = EPrimitiveType::Sphere;
            switch (SelectedBody->BodyType)
            {
            case EBodySetupType::Sphere:   NewPrimitiveType = EPrimitiveType::Sphere; break;
            case EBodySetupType::Box:      NewPrimitiveType = EPrimitiveType::Box; break;
            case EBodySetupType::Capsule:  NewPrimitiveType = EPrimitiveType::Capsule; break;
            default: break;
            }

            RecreateBodyPrimitive(SelectedBody, NewPrimitiveType);
        }
    }

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // === Transform Properties ===
    ImGui::Text("Transform");
    ImGui::Spacing();

    // Get the current primitive's center and rotation
    FVector CurrentCenter = FVector::Zero();
    FQuat CurrentRotation = FQuat::Identity();

    switch (SelectedBody->BodyType)
    {
    case EBodySetupType::Sphere:
        if (!SelectedBody->AggGeom.SphereElems.IsEmpty())
        {
            CurrentCenter = SelectedBody->AggGeom.SphereElems[0].Center;
        }
        break;
    case EBodySetupType::Box:
        if (!SelectedBody->AggGeom.BoxElems.IsEmpty())
        {
            CurrentCenter = SelectedBody->AggGeom.BoxElems[0].Center;
            CurrentRotation = SelectedBody->AggGeom.BoxElems[0].Rotation;
        }
        break;
    case EBodySetupType::Capsule:
        if (!SelectedBody->AggGeom.SphylElems.IsEmpty())
        {
            CurrentCenter = SelectedBody->AggGeom.SphylElems[0].Center;
            CurrentRotation = SelectedBody->AggGeom.SphylElems[0].Rotation;
        }
        break;
    }

    // Convert quaternion to euler angles for display (ZYX order, in degrees)
    FVector EulerAngles = CurrentRotation.ToEulerZYXDeg();

    bool bTransformChanged = false;

    if (ImGui::DragFloat3("Center", &CurrentCenter.X, 0.01f, -100.0f, 100.0f, "%.3f"))
    {
        bTransformChanged = true;
    }

    if (ImGui::DragFloat3("Rotation (Deg)", &EulerAngles.X, 0.5f, -180.0f, 180.0f, "%.2f"))
    {
        // Convert euler angles (degrees) back to quaternion
        CurrentRotation = FQuat::MakeFromEulerZYX(EulerAngles);
        bTransformChanged = true;
    }

    // Apply transform changes to the primitive
    if (bTransformChanged)
    {
        switch (SelectedBody->BodyType)
        {
        case EBodySetupType::Sphere:
            if (!SelectedBody->AggGeom.SphereElems.IsEmpty())
            {
                SelectedBody->AggGeom.SphereElems[0].Center = CurrentCenter;
            }
            break;
        case EBodySetupType::Box:
            if (!SelectedBody->AggGeom.BoxElems.IsEmpty())
            {
                SelectedBody->AggGeom.BoxElems[0].Center = CurrentCenter;
                SelectedBody->AggGeom.BoxElems[0].Rotation = CurrentRotation;
            }
            break;
        case EBodySetupType::Capsule:
            if (!SelectedBody->AggGeom.SphylElems.IsEmpty())
            {
                SelectedBody->AggGeom.SphylElems[0].Center = CurrentCenter;
                SelectedBody->AggGeom.SphylElems[0].Rotation = CurrentRotation;
            }
            break;
        }
        bChanged = true;
    }

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // === Type-Specific Properties ===
    switch (SelectedBody->BodyType)
    {
    case EBodySetupType::Sphere:
    {
        ImGui::Text("Sphere Properties");
        ImGui::Spacing();

        float PrevRadius = SelectedBody->SphereRadius;
        if (ImGui::DragFloat("Radius", &SelectedBody->SphereRadius, 0.01f, 0.01f, 5.0f, "%.3f"))
        {
            // Update AggGeom
            if (!SelectedBody->AggGeom.SphereElems.IsEmpty())
            {
                for (FSphereElem& SphereElem : SelectedBody->AggGeom.SphereElems)
                {
                    SphereElem.Radius = SelectedBody->SphereRadius;
                }
            }
            bChanged = true;
        }
        break;
    }

    case EBodySetupType::Box:
    {
        ImGui::Text("Box Properties");
        ImGui::Spacing();

        FVector PrevExtent = SelectedBody->BoxExtent;
        if (ImGui::DragFloat3("Half Extent", &SelectedBody->BoxExtent.X, 0.01f, 0.01f, 10.0f, "%.3f"))
        {
            // Update AggGeom
            if (!SelectedBody->AggGeom.BoxElems.IsEmpty())
            {
                for (FBoxElem& BoxElem : SelectedBody->AggGeom.BoxElems)
                {
                    BoxElem.X = SelectedBody->BoxExtent.X * 2.f;
                    BoxElem.Y = SelectedBody->BoxExtent.Y * 2.f;
                    BoxElem.Z = SelectedBody->BoxExtent.Z * 2.f;
                }
            }
            bChanged = true;
        }
        break;
    }

    case EBodySetupType::Capsule:
    {
        ImGui::Text("Capsule Properties");
        ImGui::Spacing();

        float PrevRadius = SelectedBody->SphereRadius;
        float PrevHalfHeight = SelectedBody->CapsuleHalfHeight;

        bool bRadiusChanged = ImGui::DragFloat("Radius", &SelectedBody->SphereRadius, 0.01f, 0.01f, 5.0f, "%.3f");
        bool bHeightChanged = ImGui::DragFloat("Half Height", &SelectedBody->CapsuleHalfHeight, 0.01f, 0.01f, 10.0f, "%.3f");

        if (bRadiusChanged || bHeightChanged)
        {
            // Update AggGeom
            if (!SelectedBody->AggGeom.SphylElems.IsEmpty())
            {
                for (FSphylElem& CapsuleElem : SelectedBody->AggGeom.SphylElems)
                {
                    if (bRadiusChanged)
                    {
                        CapsuleElem.Radius = SelectedBody->SphereRadius;
                    }
                    if (bHeightChanged)
                    {
                        CapsuleElem.Length = SelectedBody->CapsuleHalfHeight * 2.f;
                    }
                }
            }
            bChanged = true;
        }
        break;
    }

    default:
        ImGui::TextDisabled("No shape selected");
        break;
    }

    // Mark collision shapes dirty to update the visualization
    if (bChanged)
    {
        ActiveState->bIsDirty = true;
        bCollisionShapesDirty = true;
    }
}

void SPhysicsAssetEditorWindow::RenderConstraintProperties()
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset) return;
    if (ActiveState->SelectedConstraintIndex < 0) return;

    // Bounds check before accessing array
    TArray<FConstraintSetup>& ConstraintSetups = ActiveState->CurrentPhysicsAsset->GetConstraintSetupsMutable();
    if (ActiveState->SelectedConstraintIndex >= ConstraintSetups.Num())
    {
        // Selection index is out of range, reset it
        ActiveState->SelectedConstraintIndex = -1;
        return;
    }

    // Get mutable reference to the constraint (not const)
    FConstraintSetup& SelectedConstraint = ConstraintSetups[ActiveState->SelectedConstraintIndex];
    FConstraintProfileProperties& Profile = SelectedConstraint.Profile;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.30f, 0.8f));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("CONSTRAINT PROPERTIES");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));

    // Basic Info (Read-only)
    ImGui::Text("Joint Name:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", SelectedConstraint.JointName.ToString().c_str());

    ImGui::Text("Parent Bone:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", SelectedConstraint.ConstraintBone1.ToString().c_str());

    ImGui::Text("Child Bone:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", SelectedConstraint.ConstraintBone2.ToString().c_str());

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // Helper lambda for rendering EJointMotion combo
    auto RenderJointMotionCombo = [](const char* Label, EJointMotion& Motion) -> bool
    {
        const char* MotionNames[] = { "Locked", "Limited", "Free" };
        int CurrentMotion = (int)Motion;

        if (ImGui::Combo(Label, &CurrentMotion, MotionNames, IM_ARRAYSIZE(MotionNames)))
        {
            Motion = (EJointMotion)CurrentMotion;
            return true;
        }
        return false;
    };

    bool bChanged = false;

    // === Constraint Frames ===
    if (ImGui::TreeNodeEx("Constraint Frames", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();

        // Child Frame (Frame1)
        if (ImGui::TreeNodeEx("Child Frame (Frame1)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Spacing();
            bChanged |= ImGui::DragFloat3("Position##Frame1Pos", &SelectedConstraint.Frame1.Pos.X, 0.01f, -100.0f, 100.0f, "%.3f");
            bChanged |= ImGui::DragFloat3("Primary Axis##Frame1Pri", &SelectedConstraint.Frame1.PriAxis.X, 0.01f, -1.0f, 1.0f, "%.3f");
            bChanged |= ImGui::DragFloat3("Secondary Axis##Frame1Sec", &SelectedConstraint.Frame1.SecAxis.X, 0.01f, -1.0f, 1.0f, "%.3f");

            // Normalize axes if changed
            if (bChanged)
            {
                SelectedConstraint.Frame1.PriAxis = SelectedConstraint.Frame1.PriAxis.GetSafeNormal();
                SelectedConstraint.Frame1.SecAxis = SelectedConstraint.Frame1.SecAxis.GetSafeNormal();
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();

        // Parent Frame (Frame2)
        if (ImGui::TreeNodeEx("Parent Frame (Frame2)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Spacing();
            bChanged |= ImGui::DragFloat3("Position##Frame2Pos", &SelectedConstraint.Frame2.Pos.X, 0.01f, -100.0f, 100.0f, "%.3f");
            bChanged |= ImGui::DragFloat3("Primary Axis##Frame2Pri", &SelectedConstraint.Frame2.PriAxis.X, 0.01f, -1.0f, 1.0f, "%.3f");
            bChanged |= ImGui::DragFloat3("Secondary Axis##Frame2Sec", &SelectedConstraint.Frame2.SecAxis.X, 0.01f, -1.0f, 1.0f, "%.3f");

            // Normalize axes if changed
            if (bChanged)
            {
                SelectedConstraint.Frame2.PriAxis = SelectedConstraint.Frame2.PriAxis.GetSafeNormal();
                SelectedConstraint.Frame2.SecAxis = SelectedConstraint.Frame2.SecAxis.GetSafeNormal();
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    ImGui::Spacing();

    // === Angular Limits ===
    if (ImGui::TreeNodeEx("Angular Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();

        // Swing1
        bChanged |= RenderJointMotionCombo("Swing1 Motion", Profile.Swing1Motion);
        if (Profile.Swing1Motion == EJointMotion::Limited)
        {
            bChanged |= ImGui::DragFloat("Swing1 Limit", &Profile.Swing1LimitsAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
        }

        ImGui::Spacing();

        // Swing2
        bChanged |= RenderJointMotionCombo("Swing2 Motion", Profile.Swing2Motion);
        if (Profile.Swing2Motion == EJointMotion::Limited)
        {
            bChanged |= ImGui::DragFloat("Swing2 Limit", &Profile.Swing2LimitsAngle, 1.0f, 0.0f, 180.0f, "%.1f deg");
        }

        ImGui::Spacing();

        // Twist
        bChanged |= RenderJointMotionCombo("Twist Motion", Profile.TwistMotion);
        if (Profile.TwistMotion == EJointMotion::Limited)
        {
            bChanged |= ImGui::DragFloat("Twist Limit", &Profile.TwistLimit, 1.0f, 0.0f, 180.0f, "%.1f deg");
        }

        ImGui::TreePop();
    }

    ImGui::Spacing();

    // === Linear Limits ===
    if (ImGui::TreeNodeEx("Linear Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();

        // Linear X
        bChanged |= RenderJointMotionCombo("Linear X Motion", Profile.LinearMotionX);

        // Linear Y
        bChanged |= RenderJointMotionCombo("Linear Y Motion", Profile.LinearMotionY);

        // Linear Z
        bChanged |= RenderJointMotionCombo("Linear Z Motion", Profile.LinearMotionZ);

        ImGui::Spacing();

        // Linear limit (shared for all axes when limited)
        if (Profile.LinearMotionX == EJointMotion::Limited ||
            Profile.LinearMotionY == EJointMotion::Limited ||
            Profile.LinearMotionZ == EJointMotion::Limited)
        {
            bChanged |= ImGui::DragFloat("Linear Limit", &Profile.LinearLimit, 0.1f, 0.0f, 100.0f, "%.2f");
        }

        ImGui::TreePop();
    }

    ImGui::Spacing();

    // === Other Settings ===
    if (ImGui::TreeNodeEx("Other Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();

        bChanged |= ImGui::Checkbox("Disable Collision", &Profile.bDisableCollision);
        bChanged |= ImGui::Checkbox("Enable Drive", &Profile.bEnableDrive);

        ImGui::TreePop();
    }

    // If any constraint property changed, update both visualization and runtime constraints
    if (bChanged)
    {
        bCollisionShapesDirty = true;

        // If simulating, also update the runtime constraint instance
        if (bIsSimulating && ActiveState->PreviewActor)
        {
            USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
            if (MeshComp)
            {
                TArray<FConstraintInstance*> RuntimeConstraints = MeshComp->GetConstraints();

                // Find matching runtime constraint by index
                if (ActiveState->SelectedConstraintIndex >= 0 && ActiveState->SelectedConstraintIndex < RuntimeConstraints.Num())
                {
                    FConstraintInstance* RuntimeConstraint = RuntimeConstraints[ActiveState->SelectedConstraintIndex];
                    if (RuntimeConstraint && RuntimeConstraint->IsValid())
                    {
                        // Update runtime constraint frames
                        RuntimeConstraint->SetRefFrame1(SelectedConstraint.Frame1);
                        RuntimeConstraint->SetRefFrame2(SelectedConstraint.Frame2);

                        // Update runtime constraint limits
                        RuntimeConstraint->SetAngularSwing1Limit(Profile.Swing1LimitsAngle, Profile.Swing1Motion);
                        RuntimeConstraint->SetAngularSwing2Limit(Profile.Swing2LimitsAngle, Profile.Swing2Motion);
                        RuntimeConstraint->SetAngularTwistLimit(Profile.TwistLimit, Profile.TwistMotion);
                        RuntimeConstraint->SetLinearLimit(Profile.LinearLimit, Profile.LinearMotionX, Profile.LinearMotionY, Profile.LinearMotionZ);
                        RuntimeConstraint->SetDisableCollision(Profile.bDisableCollision);
                    }
                }
            }
        }
    }
}

void SPhysicsAssetEditorWindow::DrawWireframeBox(ULineComponent* LineComp, const FVector& Center, const FVector& HalfExtents, const FQuat& Rotation, const FVector4& Color)
{
    if (!LineComp) return;

    // rotated local axes
    FVector AxisX = Rotation.RotateVector(FVector(1, 0, 0));
    FVector AxisY = Rotation.RotateVector(FVector(0, 1, 0));
    FVector AxisZ = Rotation.RotateVector(FVector(0, 0, 1));

    // 8 corners of the box
    FVector corners[8] = {
        Center + AxisX * HalfExtents.X + AxisY * HalfExtents.Y + AxisZ * HalfExtents.Z,  // 0: +X +Y +Z
        Center + AxisX * HalfExtents.X + AxisY * HalfExtents.Y - AxisZ * HalfExtents.Z,  // 1: +X +Y -Z
        Center + AxisX * HalfExtents.X - AxisY * HalfExtents.Y + AxisZ * HalfExtents.Z,  // 2: +X -Y +Z
        Center + AxisX * HalfExtents.X - AxisY * HalfExtents.Y - AxisZ * HalfExtents.Z,  // 3: +X -Y -Z
        Center - AxisX * HalfExtents.X + AxisY * HalfExtents.Y + AxisZ * HalfExtents.Z,  // 4: -X +Y +Z
        Center - AxisX * HalfExtents.X + AxisY * HalfExtents.Y - AxisZ * HalfExtents.Z,  // 5: -X +Y -Z
        Center - AxisX * HalfExtents.X - AxisY * HalfExtents.Y + AxisZ * HalfExtents.Z,  // 6: -X -Y +Z
        Center - AxisX * HalfExtents.X - AxisY * HalfExtents.Y - AxisZ * HalfExtents.Z   // 7: -X -Y -Z
    };

    // 12 edges of the box
    // Bottom face (Z-)
    LineComp->AddLine(corners[1], corners[3], Color);
    LineComp->AddLine(corners[3], corners[7], Color);
    LineComp->AddLine(corners[7], corners[5], Color);
    LineComp->AddLine(corners[5], corners[1], Color);

    // Top face (Z+)
    LineComp->AddLine(corners[0], corners[2], Color);
    LineComp->AddLine(corners[2], corners[6], Color);
    LineComp->AddLine(corners[6], corners[4], Color);
    LineComp->AddLine(corners[4], corners[0], Color);

    // Vertical edges
    LineComp->AddLine(corners[0], corners[1], Color);
    LineComp->AddLine(corners[2], corners[3], Color);
    LineComp->AddLine(corners[4], corners[5], Color);
    LineComp->AddLine(corners[6], corners[7], Color);
}

void SPhysicsAssetEditorWindow::DrawWireframeSphere(ULineComponent* LineComp, const FVector& Center, float Radius, const FQuat& Rotation, const FVector4& Color, int32 Segments)
{
    if (!LineComp || Segments < 3) return;

    const float TWO_PI = 6.28318530718f;

    // Draw 3 orthogonal circles (XY, XZ, YZ planes)
    for (int32 i = 0; i < Segments; ++i)
    {
        float angle0 = (static_cast<float>(i) / static_cast<float>(Segments)) * TWO_PI;
        float angle1 = (static_cast<float>((i + 1) % Segments) / static_cast<float>(Segments)) * TWO_PI;

        float cos0 = cosf(angle0);
        float sin0 = sinf(angle0);
        float cos1 = cosf(angle1);
        float sin1 = sinf(angle1);

        // XY plane (Z = 0)
        FVector local_xy_0 = FVector(Radius * cos0, Radius * sin0, 0.0f);
        FVector local_xy_1 = FVector(Radius * cos1, Radius * sin1, 0.0f);
        FVector p0_xy = Center + Rotation.RotateVector(local_xy_0);
        FVector p1_xy = Center + Rotation.RotateVector(local_xy_1);
        LineComp->AddLine(p0_xy, p1_xy, Color);

        // XZ plane (Y = 0)
        FVector local_xz_0 = FVector(Radius * cos0, 0.0f, Radius * sin0);
        FVector local_xz_1 = FVector(Radius * cos1, 0.0f, Radius * sin1);
        FVector p0_xz = Center + Rotation.RotateVector(local_xz_0);
        FVector p1_xz = Center + Rotation.RotateVector(local_xz_1);
        LineComp->AddLine(p0_xz, p1_xz, Color);

        // YZ plane (X = 0)
        FVector local_yz_0 = FVector(0.0f, Radius * cos0, Radius * sin0);
        FVector local_yz_1 = FVector(0.0f, Radius * cos1, Radius * sin1);
        FVector p0_yz = Center + Rotation.RotateVector(local_yz_0);
        FVector p1_yz = Center + Rotation.RotateVector(local_yz_1);
        LineComp->AddLine(p0_yz, p1_yz, Color);
    }
}

void SPhysicsAssetEditorWindow::DrawWireframeCapsule(ULineComponent* LineComp, const FVector& Center, float Radius, float HalfHeight, const FQuat& Rotation, const FVector4& Color, int32 Segments)
{
    if (!LineComp || Segments < 3) return;

    // Capsule is oriented along Z-axis
    // Top/Bottom hemisphere center (local, before rotation)
    FVector LocalTop = FVector(0.0f, 0.0f, HalfHeight);
    FVector LocalBottom = FVector(0.0f, 0.0f, -HalfHeight);
    
    // Rotate top/bottom centers
    FVector RotTop = Rotation.RotateVector(LocalTop);
    FVector RotBottom = Rotation.RotateVector(LocalBottom);

    // --------------------------------------------------------
    // Draw cylinder body (4 vertical lines connecting top and bottom circles)
    // --------------------------------------------------------
    int32 numVerticalLines = 4;
    for (int32 i = 0; i < numVerticalLines; ++i)
    {
        float angle = (static_cast<float>(i) / (static_cast<float>(numVerticalLines))) * TWO_PI;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        FVector localCirclePoint = FVector(Radius * cosA, Radius * sinA, 0.0f);

        // rotate center and circle separately
        FVector p0 = Center + RotTop + Rotation.RotateVector(localCirclePoint);
        FVector p1 = Center + RotBottom + Rotation.RotateVector(localCirclePoint);

        LineComp->AddLine(p0, p1, Color);
    }

    // --------------------------------------------------------
    // Draw top and bottom circles (XY plane)
    // --------------------------------------------------------
    for (int32 i = 0; i < Segments; ++i)
    {
        float angle0 = (static_cast<float>(i) / static_cast<float>(Segments)) * TWO_PI;
        float angle1 = (static_cast<float>((i + 1) % Segments) / static_cast<float>(Segments)) * TWO_PI;

        FVector local0 = FVector(Radius * cosf(angle0), Radius * sinf(angle0), 0);
        FVector local1 = FVector(Radius * cosf(angle1), Radius * sinf(angle1), 0);

        // Rotation for circle arcs
        FVector r0 = Rotation.RotateVector(local0);
        FVector r1 = Rotation.RotateVector(local1);

        // Top circle
        LineComp->AddLine(Center + RotTop + r0, Center + RotTop + r1, Color);

        // Bottom circle
        LineComp->AddLine(Center + RotBottom + r0, Center + RotBottom + r1, Color);
    }

    // --------------------------------------------------------
    // Draw hemisphere arcs (XZ and YZ planes)
    // --------------------------------------------------------
    int32 arcSegments = Segments / 2;

    for (int32 i = 0; i < arcSegments; ++i)
    {
        float angle0 = (static_cast<float>(i) / static_cast<float>(arcSegments)) * PI;
        float angle1 = (static_cast<float>(i + 1) / static_cast<float>(arcSegments)) * PI;

        // XZ plane arc
        FVector a0 = FVector(Radius * cosf(angle0), 0, Radius * sinf(angle0));
        FVector a1 = FVector(Radius * cosf(angle1), 0, Radius * sinf(angle1));

        // Top hemisphere (XZ)
        FVector p0_top_xz = Center + RotTop + Rotation.RotateVector(a0);
        FVector p1_top_xz = Center + RotTop + Rotation.RotateVector(a1);
        LineComp->AddLine(p0_top_xz, p1_top_xz, Color);

        // Bottom hemisphere (XZ)
        FVector a0b = FVector(a0.X, a0.Y, -a0.Z);
        FVector a1b = FVector(a1.X, a1.Y, -a1.Z);
        FVector p0_bottom_xz = Center + RotBottom + Rotation.RotateVector(a0b);
        FVector p1_bottom_xz = Center + RotBottom + Rotation.RotateVector(a1b);
        LineComp->AddLine(p0_bottom_xz, p1_bottom_xz, Color);

        // YZ plane arc
        FVector b0 = FVector(0, Radius * cosf(angle0), Radius * sinf(angle0));
        FVector b1 = FVector(0, Radius * cosf(angle1), Radius * sinf(angle1));

        // Top hemisphere (YZ)
        FVector p0_top_yz = Center + RotTop + Rotation.RotateVector(b0);
        FVector p1_top_yz = Center + RotTop + Rotation.RotateVector(b1);
        LineComp->AddLine(p0_top_yz, p1_top_yz, Color);

        // Bottom hemisphere (YZ)
        FVector b0b = FVector(b0.X, b0.Y, -b0.Z);
        FVector b1b = FVector(b1.X, b1.Y, -b1.Z);
        FVector p0_bottom_yz = Center + RotBottom + Rotation.RotateVector(b0b);
        FVector p1_bottom_yz = Center + RotBottom + Rotation.RotateVector(b1b);
        LineComp->AddLine(p0_bottom_yz, p1_bottom_yz, Color);
    }
}

void SPhysicsAssetEditorWindow::DrawWireframeConvex(ULineComponent* LineComp, const FConvexElem& ConvexElem, const FTransform WorldTransform, const FVector4& Color)
{
    if (!LineComp)
    {
        return;
    }

    if (ConvexElem.VertexData.IsEmpty())
    {
        return;
    }

    if (ConvexElem.IndexData.IsEmpty())
    {
        // Fallback: 정점만 포인트로 찍기 (디버깅용)
        for (const FVector& LocalVert : ConvexElem.VertexData)
        {
            FVector WorldVert = WorldTransform.TransformPosition(LocalVert);
            // 점 그리기 함수가 있다고 가정 (없으면 짧은 선으로 대체)
            // LineComp->AddPoint(WorldVert, Color, 5.0f); 
        }
        return;
    }

    const TArray<int32>& Indices = ConvexElem.IndexData;
    const TArray<FVector>& Vertices = ConvexElem.VertexData;

    for (int32 i = 0; i < Indices.Num(); i += 3)
    {
        // 안전 장치
        if (i + 2 >= Indices.Num()) break;

        int32 Index0 = Indices[i];
        int32 Index1 = Indices[i + 1];
        int32 Index2 = Indices[i + 2];

        // 인덱스 범위 체크
        if ((Index0 >= 0) && (Index0 < Vertices.Num()) ||
            (Index1 >= 0) && (Index1 < Vertices.Num()) ||
            (Index2 >= 0) && (Index2 < Vertices.Num()))
        {
            continue;
        }

        // 3. 로컬 정점을 월드 좌표로 변환
        FVector V0 = WorldTransform.TransformPosition(Vertices[Index0]);
        FVector V1 = WorldTransform.TransformPosition(Vertices[Index1]);
        FVector V2 = WorldTransform.TransformPosition(Vertices[Index2]);

        // 4. 삼각형의 세 변(Edge)을 그리기
        // (중복해서 그리는 선분이 생기지만, 디버그 드로잉에서는 무시해도 되는 수준의 오버헤드입니다)
        LineComp->AddLine(V0, V1, Color);
        LineComp->AddLine(V1, V2, Color);
        LineComp->AddLine(V2, V0, Color);
    }
}

void SPhysicsAssetEditorWindow::RebuildCollisionShapes()
{
    if (!ActiveState)   return;

    // Use the dedicated collision shape LineComponent from ViewerState
    ULineComponent* LineComp = ActiveState->CollisionShapeLineComponent;
    if (!LineComp)  return;

    // Clear previous collision shape lines
    ClearCollisionShapes();

    // Check if we have a PhysicsAsset with bodies
    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    if (!PhysicsAsset || PhysicsAsset->GetBodySetupCount() == 0)
    {
        return;
    }

    // Get skeletal mesh component for bone transforms
    ASkeletalMeshActor* PreviewActor = ActiveState->PreviewActor;
    if (!PreviewActor)
    {
        return;
    }

    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)
    {
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton)
    {
        return;
    }

    // Unreal Engine collision purple color
    FVector4 CollisionColor(0.6f, 0.3f, 0.8f, 1.0f);
    FVector4 SelectedColor(1.0f, 0.5f, 0.0f, 1.0f);  // Orange for selected body

    // Iterate through all bodies in PhysicsAsset
    const TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
    for (int32 BodyIndex = 0; BodyIndex < BodySetups.Num(); ++BodyIndex)
    {
        UBodySetup* BodySetup = BodySetups[BodyIndex];
        if (!BodySetup)
        {
            continue;
        }

        // Find bone index for this body
        int32 BoneIndex = -1;
        for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
        {
            if (Skeleton->Bones[i].Name == BodySetup->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }

        if (BoneIndex < 0)
        {
            continue;
        }

        // Get bone world transform
        FTransform BoneTransform = MeshComp->GetBoneWorldTransform(BoneIndex);
        BoneTransform.Scale3D = FVector(1, 1, 1);   // Remove Scaling!!!

        FVector BoneWorldPos = BoneTransform.Translation;
        FQuat BoneWorldRot = BoneTransform.Rotation;

        // Choose color based on selection
        FVector4 DrawColor = (BodyIndex == ActiveState->SelectedBodyIndex) ? SelectedColor : CollisionColor;

        // Draw spheres
        for (const FSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
        {
            // Transform center to world space
            FVector WorldCenter = BoneTransform.TransformPosition(SphereElem.Center);
            FQuat WorldRotation = BoneWorldRot;
            DrawWireframeSphere(LineComp, WorldCenter, SphereElem.Radius, WorldRotation, DrawColor, 16);
        }

        // Draw boxes
        for (const FBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
        {
            // Transform center and rotation to world space
            FVector WorldCenter = BoneTransform.TransformPosition(BoxElem.Center);
            FQuat WorldRotation = BoneWorldRot * BoxElem.Rotation;

            // Box half extents (BoxElem stores full dimensions)
            FVector HalfExtents(BoxElem.X * 0.5f, BoxElem.Y * 0.5f, BoxElem.Z * 0.5f);

            // Draw box with rotation and transformed center
            DrawWireframeBox(LineComp, WorldCenter, HalfExtents, WorldRotation, DrawColor);
        }

        // Draw capsules
        for (const FSphylElem& CapsuleElem : BodySetup->AggGeom.SphylElems)
        {
            // Transform center and rotation to world space
            FVector WorldCenter = BoneTransform.TransformPosition(CapsuleElem.Center);
            FQuat WorldRotation = BoneWorldRot * CapsuleElem.Rotation;

            // Capsule half height (Length is cylinder portion only)
            float HalfHeight = CapsuleElem.Length * 0.5f;

            // Draw capsule with rotation (need to implement rotated capsule drawing)
            // For now, draw axis-aligned capsule at transformed center
            DrawWireframeCapsule(LineComp, WorldCenter, CapsuleElem.Radius, HalfHeight, WorldRotation, DrawColor, 16);
        }

        for (const FConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
        {
            // 1. ConvexElem 자체의 로컬 변환 (Transform) 적용
            // (보통 Identity지만, 데이터 생성 시 오프셋이 들어갔을 수 있음)
            FTransform ElemTransform = ConvexElem.GetTransform(); // 혹은 멤버 변수 Transform

            // 2. 최종 월드 변환 계산 (Bone World * Elem Local)
            FTransform FinalWorldTransform = BoneTransform.GetWorldTransform(ElemTransform);
    
            // (참고: FTransform 구현에 따라 곱셈 순서가 다를 수 있습니다. 
            // 님의 FTransform::GetWorldTransform은 Parent.GetWorldTransform(Child) 방식이므로
            // BoneTransform이 Parent입니다.)

            // 3. 그리기 함수 호출
            DrawWireframeConvex(LineComp, ConvexElem, FinalWorldTransform, DrawColor);
        }
    }

    // Rebuild constraint visualization
    RebuildConstraintVisualization();

    bCollisionShapesDirty = false;
}

void SPhysicsAssetEditorWindow::ClearCollisionShapes()
{
    if (!ActiveState) return;

    // Use the dedicated collision shape LineComponent from ViewerState
    ULineComponent* LineComp = ActiveState->CollisionShapeLineComponent;
    if (!LineComp) return;

    // Clear only collision shape lines
    LineComp->ClearLines();
}

void SPhysicsAssetEditorWindow::DrawConstraintFrame(ULineComponent* LineComp, const FVector& Position, const FVector& PriAxis, const FVector& SecAxis, float AxisLength, const FVector4& Color)
{
    if (!LineComp) return;

    // Calculate third axis (normal)
    FVector NormalAxis = FVector::Cross(PriAxis, SecAxis).GetSafeNormal();

    // Draw three axes
    FVector4 RedColor(1.0f, 0.0f, 0.0f, 1.0f);    // X - PriAxis (Twist axis)
    FVector4 GreenColor(0.0f, 1.0f, 0.0f, 1.0f);  // Y - SecAxis
    FVector4 BlueColor(0.0f, 0.0f, 1.0f, 1.0f);   // Z - Normal

    LineComp->AddLine(Position, Position + PriAxis * AxisLength, RedColor);
    LineComp->AddLine(Position, Position + SecAxis * AxisLength, GreenColor);
    LineComp->AddLine(Position, Position + NormalAxis * AxisLength, BlueColor);
}

void SPhysicsAssetEditorWindow::DrawSwingCone(ULineComponent* LineComp, const FVector& Position, const FVector& PriAxis, const FVector& SecAxis, float Swing1Angle, float Swing2Angle, const FVector4& Color, int32 Segments)
{
    if (!LineComp || Segments < 3) return;

    // Convert angles from degrees to radians
    float Swing1Rad = DegreesToRadians(Swing1Angle);
    float Swing2Rad = DegreesToRadians(Swing2Angle);

    // Calculate cone height (small scale for visualization relative to bone size)
    float ConeLength = 0.1f;

    // Calculate normal axis (perpendicular to both PriAxis and SecAxis)
    FVector NormalAxis = FVector::Cross(PriAxis, SecAxis).GetSafeNormal();

    // Draw elliptical cone
    // The cone opens along PriAxis, with Swing1 in SecAxis direction and Swing2 in NormalAxis direction
    TArray<FVector> CirclePoints;
    CirclePoints.reserve(Segments);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Angle = (static_cast<float>(i) / static_cast<float>(Segments)) * TWO_PI;
        float CosAngle = cosf(Angle);
        float SinAngle = sinf(Angle);

        // Ellipse radii based on swing angles
        float RadiusY = ConeLength * tanf(Swing1Rad);  // Swing1 direction (SecAxis)
        float RadiusZ = ConeLength * tanf(Swing2Rad);  // Swing2 direction (NormalAxis)

        // Point on ellipse
        FVector LocalPoint = SecAxis * (RadiusY * CosAngle) + NormalAxis * (RadiusZ * SinAngle);
        FVector WorldPoint = Position + PriAxis * ConeLength + LocalPoint;
        CirclePoints.Add(WorldPoint);
    }

    // Draw cone edge circle
    for (int32 i = 0; i < Segments; ++i)
    {
        LineComp->AddLine(CirclePoints[i], CirclePoints[i + 1], Color);
    }

    // Draw radial lines from apex to circle (4 lines at cardinal directions)
    int32 NumRadialLines = 4;
    for (int32 i = 0; i < NumRadialLines; ++i)
    {
        int32 Index = (i * Segments) / NumRadialLines;
        LineComp->AddLine(Position, CirclePoints[Index], Color);
    }
}

void SPhysicsAssetEditorWindow::DrawTwistArc(ULineComponent* LineComp, const FVector& Position, const FVector& PriAxis, const FVector& SecAxis, float TwistAngle, float Radius, const FVector4& Color, int32 Segments)
{
    if (!LineComp || Segments < 3) return;

    // Convert angle from degrees to radians
    float TwistRad = DegreesToRadians(TwistAngle);

    // Draw arc around PriAxis
    // Arc goes from -TwistAngle to +TwistAngle

    TArray<FVector> ArcPoints;
    ArcPoints.reserve(Segments + 1);

    // Calculate normal axis
    FVector NormalAxis = FVector::Cross(PriAxis, SecAxis).GetSafeNormal();

    // Generate arc points
    for (int32 i = 0; i <= Segments; ++i)
    {
        // Angle ranges from -TwistRad to +TwistRad
        float t = static_cast<float>(i) / static_cast<float>(Segments);
        float CurrentAngle = FMath::Lerp(-TwistRad, TwistRad, t);

        // Rotate SecAxis around PriAxis
        FVector RotatedDir = SecAxis * cosf(CurrentAngle) + NormalAxis * sinf(CurrentAngle);
        FVector ArcPoint = Position + RotatedDir * Radius;
        ArcPoints.Add(ArcPoint);
    }

    // Draw arc
    for (int32 i = 0; i < Segments; ++i)
    {
        LineComp->AddLine(ArcPoints[i], ArcPoints[i + 1], Color);
    }

    // Draw lines from center to arc endpoints to show the limit
    LineComp->AddLine(Position, ArcPoints[0], Color);
    LineComp->AddLine(Position, ArcPoints[Segments], Color);
}

void SPhysicsAssetEditorWindow::RebuildConstraintVisualization()
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset) return;

    ULineComponent* LineComp = ActiveState->CollisionShapeLineComponent;
    if (!LineComp) return;

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    const TArray<FConstraintSetup>& Constraints = PhysicsAsset->GetContraintSetups();

    if (Constraints.IsEmpty()) return;

    ASkeletalMeshActor* PreviewActor = ActiveState->PreviewActor;
    if (!PreviewActor) return;

    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    // Build BoneName -> BoneIndex map for performance
    TMap<FString, int32> BoneNameToIndexMap;
    for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        BoneNameToIndexMap.Add(Skeleton->Bones[i].Name, i);
    }

    // Colors
    FVector4 ConstraintColor(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan for constraints
    FVector4 SelectedColor(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow for selected

    for (int32 i = 0; i < Constraints.Num(); ++i)
    {
        const FConstraintSetup& Constraint = Constraints[i];

        // Find child bone index
        int32* ChildBoneIndexPtr = BoneNameToIndexMap.Find(Constraint.ConstraintBone1.ToString());
        if (!ChildBoneIndexPtr) continue;
        int32 ChildBoneIndex = *ChildBoneIndexPtr;

        // Get child bone world transform
        FTransform ChildBoneTransform = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
        ChildBoneTransform.Scale3D = FVector(1, 1, 1);

        // Transform Frame1 to world space
        FVector ConstraintWorldPos = ChildBoneTransform.TransformPosition(Constraint.Frame1.Pos);
        FVector WorldPriAxis = ChildBoneTransform.TransformVector(Constraint.Frame1.PriAxis).GetSafeNormal();
        FVector WorldSecAxis = ChildBoneTransform.TransformVector(Constraint.Frame1.SecAxis).GetSafeNormal();

        // Choose color based on selection
        FVector4 DrawColor = (i == ActiveState->SelectedConstraintIndex) ? SelectedColor : ConstraintColor;

        // Draw constraint frame (coordinate axes)
        DrawConstraintFrame(LineComp, ConstraintWorldPos, WorldPriAxis, WorldSecAxis, 0.03f, DrawColor);

        const FConstraintProfileProperties& Profile = Constraint.Profile;

        // Draw Swing Cone if limited
        if (Profile.Swing1Motion == EJointMotion::Limited || Profile.Swing2Motion == EJointMotion::Limited)
        {
            float Swing1 = (Profile.Swing1Motion == EJointMotion::Limited) ? Profile.Swing1LimitsAngle : 180.0f;
            float Swing2 = (Profile.Swing2Motion == EJointMotion::Limited) ? Profile.Swing2LimitsAngle : 180.0f;

            DrawSwingCone(LineComp, ConstraintWorldPos, WorldPriAxis, WorldSecAxis, Swing1, Swing2, DrawColor, 24);
        }

        // Draw Twist Arc if limited
        if (Profile.TwistMotion == EJointMotion::Limited)
        {
            DrawTwistArc(LineComp, ConstraintWorldPos, WorldPriAxis, WorldSecAxis, Profile.TwistLimit, 0.1f, DrawColor, 16);
        }
    }
}

bool SPhysicsAssetEditorWindow::HasBodyInSubtree(int32 BoneIndex, const TArray<FBone>& Bones, const TArray<TArray<int32>>& Children) const
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset)
        return false;

    // Check if current bone has a physics body
    FName BoneName(Bones[BoneIndex].Name);
    if (ActiveState->CurrentPhysicsAsset->FindBodyIndex(BoneName) >= 0)
        return true;

    // Recursively check all children
    for (int32 Child : Children[BoneIndex])
    {
        if (HasBodyInSubtree(Child, Bones, Children))
            return true;
    }

    return false;
}

void SPhysicsAssetEditorWindow::CreateBodyForBone(int32 BoneIndex, EPrimitiveType PrimitiveType)
{
    if (!ActiveState || !ActiveState->CurrentMesh)
        return;

    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)
        return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
        return;

    // If no PhysicsAsset exists yet, create one.
    if (!ActiveState->CurrentPhysicsAsset)
    {
        ActiveState->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();
    }

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    const FBone& Bone = Skeleton->Bones[BoneIndex];
    FName BoneName(Bone.Name);

    // Check if a body already exists for this bone
    if (PhysicsAsset->FindBodyIndex(BoneName) >= 0)
    {
        UE_LOG("CreateBodyForBone: Bone '%s' already has a physics body.", Bone.Name.c_str());
        return;
    }

    UE_LOG("CreateBodyForBone: Creating body for bone '%s' (Index: %d)", Bone.Name.c_str(), BoneIndex);

    // ------------------------------------------------------
    // Calculate center & rotation (bone-local, scale-free)
    // ------------------------------------------------------
    FVector LocalCenter;
    FQuat LocalRotation;
    CalculateBoneLocalShapeTransform(BoneIndex, Skeleton, MeshComp, LocalCenter, LocalRotation);

    // ------------------------------------------------------
    // Calculate bone length
    // ------------------------------------------------------
    float Radius, HalfHeight;
    FVector Extent;
    CalculateBodyDimensions(BoneIndex, Skeleton, MeshComp, PrimitiveType,
                            Radius, HalfHeight, Extent);

    // Create BodySetup and primitives
    UBodySetup* NewBody = NewObject<UBodySetup>();
    if (!NewBody) return;

    NewBody->BoneName = BoneName;
    NewBody->BodyType = ToBodySetupType(PrimitiveType);

    // Add primitive element based on type
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
    {
        FSphereElem SphereElem(Radius);
        SphereElem.Center = LocalCenter;
        NewBody->AggGeom.SphereElems.Add(SphereElem);
        break;
    }
    case EPrimitiveType::Box:
    {
        FBoxElem BoxElem(Extent.X * 2, Extent.Y * 2, Extent.Z * 2);
        BoxElem.Center = LocalCenter;
        BoxElem.Rotation = LocalRotation;
        NewBody->AggGeom.BoxElems.Add(BoxElem);
        break;
    }
    case EPrimitiveType::Capsule:
    {
        FSphylElem CapsuleElem(Radius, HalfHeight * 2.f);
        CapsuleElem.Center = LocalCenter;
        CapsuleElem.Rotation = LocalRotation;
        NewBody->AggGeom.SphylElems.Add(CapsuleElem);
        break;
    }
    case EPrimitiveType::Convex:
        {
            FConvexElem ConvexElem;
            

            // 1. 계산된 Extent(Half-Size)를 이용하여 박스 형태의 정점 8개 생성
            // (이미 LocalCenter와 LocalRotation이 계산되어 있으므로, 
            //  정점은 원점 기준으로 만들고 나중에 Transform을 적용하거나, 정점에 미리 적용할 수 있습니다.)
    
            TArray<FVector> BoxVertices;
            BoxVertices.SetNum(8);
    
            // Extent는 Half-Size이므로 +/- Extent로 정점 생성
            BoxVertices[0] = FVector( Extent.X,  Extent.Y,  Extent.Z);
            BoxVertices[1] = FVector( Extent.X,  Extent.Y, -Extent.Z);
            BoxVertices[2] = FVector( Extent.X, -Extent.Y,  Extent.Z);
            BoxVertices[3] = FVector( Extent.X, -Extent.Y, -Extent.Z);
            BoxVertices[4] = FVector(-Extent.X,  Extent.Y,  Extent.Z);
            BoxVertices[5] = FVector(-Extent.X,  Extent.Y, -Extent.Z);
            BoxVertices[6] = FVector(-Extent.X, -Extent.Y,  Extent.Z);
            BoxVertices[7] = FVector(-Extent.X, -Extent.Y, -Extent.Z);

            // 2. 로컬 변환 적용 (LocalCenter, LocalRotation)
            // BoxElem은 Center/Rotation 변수가 있지만, ConvexElem은 보통 정점 데이터 자체에 굽거나 Transform 변수를 씁니다.
            // 여기서는 정점 데이터 자체를 이동시켜서 "Bone Local Space"에 맞춥니다.
            for (FVector& Vert : BoxVertices)
            {
                Vert = LocalRotation.RotateVector(Vert) + LocalCenter;
            }

            // 3. Convex Hull 생성
            if (FPhysicsUtils::GenerateConvexHull(BoxVertices, ConvexElem))
            {
                // 이미 정점을 이동시켰으므로 추가 Transform은 Identity
                ConvexElem.SetTransform(FTransform());
                NewBody->AggGeom.ConvexElems.Add(ConvexElem);
            }
            break;
        }
    default:
        UE_LOG("CreateBodyForBone: Unknown primitive type.");
        break;
    }

    // Add the body to the physics asset
    PhysicsAsset->AddBodySetup(NewBody);

    // Also populate the editor-facing properties for UI consistency
    NewBody->SphereRadius = Radius;
    NewBody->BoxExtent = Extent; // Extent is already half-extent from CalculateBodyDimensions
    NewBody->CapsuleHalfHeight = HalfHeight;

    PhysicsAsset->UpdateBodySetupIndexMap();

    // Select the newly created body
    ActiveState->SelectedBoneIndex = BoneIndex;
    ActiveState->SelectedBodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
    ActiveState->bBoneLinesDirty = true;
    bCollisionShapesDirty = true;

    ActiveState->bIsDirty = true;

    UE_LOG("CreateBodyForBone: Successfully created body for '%s'.", Bone.Name.c_str());
}

void SPhysicsAssetEditorWindow::GenerateAllBodies(EPrimitiveType PrimitiveType)
{
    if (bUseBoneLengthGeneration)
    {
        GenerateBodiesByBoneStructure(PrimitiveType);
    }
    else
    {
        GenerateBodiesByVertexFitting(PrimitiveType);
    }
}

void SPhysicsAssetEditorWindow::GenerateBodiesByBoneStructure(EPrimitiveType PrimitiveType)
{
    if (!ActiveState || !ActiveState->CurrentMesh)
        return;

    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)
        return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.IsEmpty())
        return;

    // Create & Clear all existing bodies before regenerating
    if (!ActiveState->CurrentPhysicsAsset)
        ActiveState->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();
    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    PhysicsAsset->ClearAllBodies();

    int32 CreatedCount = 0;

    // Create body for all bones
    for (int32 BoneIndex = 0; BoneIndex < Skeleton->Bones.size(); ++BoneIndex)
    {
        const FBone& Bone = Skeleton->Bones[BoneIndex];
        FName BoneName(Bone.Name);

        // Filter: Skip bones that shouldn't have physics bodies
        if (!ShouldCreateBodyForBone(BoneIndex, Skeleton))
            continue;

        // Check if this bone has enough influenced vertices
        if (PhysicsAsset->FindBodyIndex(BoneName) >= 0)
            continue;

        // Same as CreateBodyForBone
        FVector LocalCenter;
        FQuat LocalRotation;
        CalculateBoneLocalShapeTransform(BoneIndex, Skeleton, MeshComp, LocalCenter, LocalRotation);

        float Radius, HalfHeight;
        FVector Extent;
        CalculateBodyDimensions(BoneIndex, Skeleton, MeshComp, PrimitiveType,
                                Radius, HalfHeight, Extent);

        UBodySetup* NewBody = NewObject<UBodySetup>();
        if (!NewBody)
            continue;

        NewBody->BoneName = BoneName;
        NewBody->BodyType = ToBodySetupType(PrimitiveType);

        switch (PrimitiveType)
        {
        case EPrimitiveType::Sphere:
        {
            FSphereElem SphereElem(Radius);
            SphereElem.Center = LocalCenter;
            NewBody->AggGeom.SphereElems.Add(SphereElem);
            break;
        }

        case EPrimitiveType::Box:
        {
            FBoxElem BoxElem(Extent.X * 2.f, Extent.Y * 2.f, Extent.Z * 2.f);
            BoxElem.Center = LocalCenter;
            BoxElem.Rotation = LocalRotation;
            NewBody->AggGeom.BoxElems.Add(BoxElem);
            break;
        }

        case EPrimitiveType::Capsule:
        {
            FSphylElem CapsuleElem(Radius, HalfHeight * 2.f);
            CapsuleElem.Center = LocalCenter;
            CapsuleElem.Rotation = LocalRotation;
            NewBody->AggGeom.SphylElems.Add(CapsuleElem);
            break;
        }
        case EPrimitiveType::Convex:
            {
                FConvexElem ConvexElem;

                // 1. 계산된 Extent(Half-Size)를 이용하여 박스 형태의 정점 8개 생성
                // (이미 LocalCenter와 LocalRotation이 계산되어 있으므로, 
                //  정점은 원점 기준으로 만들고 나중에 Transform을 적용하거나, 정점에 미리 적용할 수 있습니다.)
    
                TArray<FVector> BoxVertices;
                BoxVertices.SetNum(8);
    
                // Extent는 Half-Size이므로 +/- Extent로 정점 생성
                BoxVertices[0] = FVector( Extent.X,  Extent.Y,  Extent.Z);
                BoxVertices[1] = FVector( Extent.X,  Extent.Y, -Extent.Z);
                BoxVertices[2] = FVector( Extent.X, -Extent.Y,  Extent.Z);
                BoxVertices[3] = FVector( Extent.X, -Extent.Y, -Extent.Z);
                BoxVertices[4] = FVector(-Extent.X,  Extent.Y,  Extent.Z);
                BoxVertices[5] = FVector(-Extent.X,  Extent.Y, -Extent.Z);
                BoxVertices[6] = FVector(-Extent.X, -Extent.Y,  Extent.Z);
                BoxVertices[7] = FVector(-Extent.X, -Extent.Y, -Extent.Z);

                // 2. 로컬 변환 적용 (LocalCenter, LocalRotation)
                // BoxElem은 Center/Rotation 변수가 있지만, ConvexElem은 보통 정점 데이터 자체에 굽거나 Transform 변수를 씁니다.
                // 여기서는 정점 데이터 자체를 이동시켜서 "Bone Local Space"에 맞춥니다.
                for (FVector& Vert : BoxVertices)
                {
                    Vert = LocalRotation.RotateVector(Vert) + LocalCenter;
                }

                // 3. Convex Hull 생성
                if (FPhysicsUtils::GenerateConvexHull(BoxVertices, ConvexElem))
                {
                    // 이미 정점을 이동시켰으므로 추가 Transform은 Identity
                    ConvexElem.SetTransform(FTransform());
                    NewBody->AggGeom.ConvexElems.Add(ConvexElem);
                }
                break;
            }
        }

        // Add the body to the physics asset
        PhysicsAsset->AddBodySetup(NewBody);
        ++CreatedCount;
    }

    // Update the body index map
    PhysicsAsset->UpdateBodySetupIndexMap();
    UE_LOG("GenerateAllBodies: Created %d bodies", CreatedCount);

    // Mark collision shapes dirty to visualize the newly created bodies
    ActiveState->bBoneLinesDirty = true;
    bCollisionShapesDirty = true;
}

void SPhysicsAssetEditorWindow::GenerateBodiesByVertexFitting(EPrimitiveType PrimitiveType)
{
    if (!ActiveState || !ActiveState->CurrentMesh)
        return;

    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)
        return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.IsEmpty())
        return;

    // Get skeletal mesh data for vertex information
    const FSkeletalMeshData* MeshData = ActiveState->CurrentMesh->GetSkeletalMeshData();
    if (!MeshData || MeshData->Vertices.Num() == 0)
        return;

    // Prepare PhysicsAsset 
    if (!ActiveState->CurrentPhysicsAsset)
        ActiveState->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();

    // Clear all existing bodies before regenerating
    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    PhysicsAsset->ClearAllBodies();

    // Step 1: Build bone-to-vertex influence map
    TArray<FBoneVertexInfluence> InfluenceMap;
    BuildBoneVertexInfluenceMap(MeshData, InfluenceMap, 0.3f);  // 30% weight threshold

    // Step 2: Track bones that are absorbed into parent bodies
    // (e.g., clavicle absorbed into spine_05, so don't create separate clavicle body)
    TArray<int32> AbsorbedBones;

    // Step 3: Create bodies for bones with sufficient vertex influence
    const TArray<FBone>& Bones = Skeleton->Bones;
    int32 CreatedCount = 0;

    for (int32 BoneIndex = 0; BoneIndex < Bones.size(); ++BoneIndex)
    {
        const FBone& Bone = Bones[BoneIndex];
        FName BoneName(Bone.Name);

        // Check influence map range
        if (BoneIndex < 0 || BoneIndex >= InfluenceMap.Num())
            continue;

        // Skip if this bone was absorbed into a parent body
        if (AbsorbedBones.Contains(BoneIndex))
        {
            UE_LOG("GenerateAllBodies: Skipping bone '%s' - already absorbed into parent body", Bone.Name.c_str());
            continue;
        }

        // Apply same filtering logic as bone-length generation
        if (!ShouldCreateBodyForBone(BoneIndex, Skeleton))
            continue;

        // For vertex-driven generation, also check vertex count
        const FBoneVertexInfluence& Influence = InfluenceMap[BoneIndex];

        FString BoneName_Lower = Bone.Name;
        std::transform(BoneName_Lower.begin(), BoneName_Lower.end(), BoneName_Lower.begin(), ::tolower);

        // Exception: neck should always create body (even with few vertices) to absorb head
        bool bIsNeck = (BoneName_Lower.find("neck") != FString::npos);

        if (!bIsNeck && Influence.Vertices.Num() < 50)  // Need significant vertex influence for meaningful shape
        {
            continue;
        }

        // Collect vertices for fitting
        // For hand/foot/pelvis/neck, include descendant/child bone vertices
        TArray<FVector> VerticesToFit = Influence.Vertices;

        bool bIsHandOrFoot =
            (BoneName_Lower.find("hand") != FString::npos) ||
            (BoneName_Lower.find("foot") != FString::npos);

        bool bIsPelvis =
            (BoneName_Lower.find("pelvis") != FString::npos) ||
            (BoneName_Lower.find("hips") != FString::npos);

        if (bIsHandOrFoot)
        {
            // Include all descendant bones' vertices (fingers/toes)
            TArray<FString> AbsorbedBoneNames;
            for (int32 i = 0; i < Bones.size(); ++i)
            {
                if (IsDescendantOf(i, BoneIndex, Bones) && i < InfluenceMap.Num())
                {
                    VerticesToFit.Append(InfluenceMap[i].Vertices);
                    // Mark descendant as absorbed (don't create separate body)
                    if (!AbsorbedBones.Contains(i))
                    {
                        AbsorbedBones.Add(i);
                        AbsorbedBoneNames.Add(Bones[i].Name);
                    }
                }
            }

            FString AbsorbedList = "";
            for (int32 idx = 0; idx < AbsorbedBoneNames.Num(); ++idx)
            {
                if (idx > 0) AbsorbedList += ", ";
                AbsorbedList += AbsorbedBoneNames[idx];
            }

            UE_LOG("GenerateAllBodies: Hand/Foot '%s' - expanded to %d vertices, absorbed %d bones: [%s]",
                Bone.Name.c_str(), VerticesToFit.Num(), AbsorbedBoneNames.Num(), AbsorbedList.c_str());
        }
        else if (bIsPelvis)
        {
            // Include direct children's vertices, but only those near pelvis
            // (to avoid spine vertices from extending too far up)
            TArray<int32> Children;
            GetAllChildBones(BoneIndex, Skeleton, Children);

            // Get pelvis world position
            FTransform PelvisWorldTM = MeshComp->GetBoneWorldTransform(BoneIndex);
            PelvisWorldTM.Scale3D = FVector(1, 1, 1);
            FVector PelvisPos = PelvisWorldTM.Translation;

            // Calculate max distance based on child bone positions
            float MaxChildDist = 0.f;
            for (int32 ChildIdx : Children)
            {
                FTransform ChildTM = MeshComp->GetBoneWorldTransform(ChildIdx);
                ChildTM.Scale3D = FVector(1, 1, 1);
                float Dist = (ChildTM.Translation - PelvisPos).Size();
                MaxChildDist = FMath::Max(MaxChildDist, Dist);
            }

            // Only include vertices within ~2.0x the max child bone distance
            float MaxVertexDistance = MaxChildDist * 2.0f;

            int32 VertexCountBefore = VerticesToFit.Num();
            for (int32 ChildIdx : Children)
            {
                if (ChildIdx < InfluenceMap.Num())
                {
                    for (const FVector& Vert : InfluenceMap[ChildIdx].Vertices)
                    {
                        if ((Vert - PelvisPos).Size() <= MaxVertexDistance)
                        {
                            VerticesToFit.Add(Vert);
                        }
                    }
                }
            }
            UE_LOG("GenerateAllBodies: Pelvis '%s' - expanded from %d to %d vertices (distance-filtered)",
                Bone.Name.c_str(), VertexCountBefore, VerticesToFit.Num());
        }
        else if (bIsNeck)
        {
            // Neck should absorb head (descendant) to create natural head capsule
            TArray<FString> AbsorbedBoneNames;
            for (int32 i = 0; i < Bones.size(); ++i)
            {
                if (IsDescendantOf(i, BoneIndex, Bones) && i < InfluenceMap.Num())
                {
                    VerticesToFit.Append(InfluenceMap[i].Vertices);
                    // Mark descendant as absorbed (don't create separate body)
                    if (!AbsorbedBones.Contains(i))
                    {
                        AbsorbedBones.Add(i);
                        AbsorbedBoneNames.Add(Bones[i].Name);
                    }
                }
            }

            FString AbsorbedList = "";
            for (int32 idx = 0; idx < AbsorbedBoneNames.Num(); ++idx)
            {
                if (idx > 0) AbsorbedList += ", ";
                AbsorbedList += AbsorbedBoneNames[idx];
            }

            UE_LOG("GenerateAllBodies: Neck '%s' - expanded to %d vertices, absorbed %d bones: [%s]",
                Bone.Name.c_str(), VerticesToFit.Num(), AbsorbedBoneNames.Num(), AbsorbedList.c_str());
        }
        else
        {
            // Check if this is a spine/chest bone (torso)
            bool bIsSpine =
                (BoneName_Lower.find("spine") != FString::npos) ||
                (BoneName_Lower.find("chest") != FString::npos);

            if (bIsSpine)
            {
                // Spine bones should encompass nearby torso structures:
                // - Upper spine (spine_05) → includes clavicles, scapulae
                // - Mid spine (spine_03/04) → includes ribs, neck base
                // - Lower spine (spine_01/02) → includes lower torso

                TArray<int32> Children;
                GetAllChildBones(BoneIndex, Skeleton, Children);

                FTransform SpineWorldTM = MeshComp->GetBoneWorldTransform(BoneIndex);
                SpineWorldTM.Scale3D = FVector(1, 1, 1);
                FVector SpinePos = SpineWorldTM.Translation;

                // Find sibling bones and children to determine coverage area
                // Siblings: bones with same parent (e.g., clavicle_l/r share spine_05 parent)
                TArray<int32> Siblings;
                int32 ParentIdx = Bone.ParentIndex;
                if (ParentIdx >= 0)
                {
                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        if (i != BoneIndex && Bones[i].ParentIndex == ParentIdx)
                        {
                            Siblings.Add(i);
                        }
                    }
                }

                // Calculate max distance from spine to related bones
                float MaxRelatedBoneDist = 0.f;

                // Check children bones (e.g., next spine segment, neck)
                for (int32 ChildIdx : Children)
                {
                    FTransform ChildTM = MeshComp->GetBoneWorldTransform(ChildIdx);
                    ChildTM.Scale3D = FVector(1, 1, 1);
                    float Dist = (ChildTM.Translation - SpinePos).Size();
                    MaxRelatedBoneDist = FMath::Max(MaxRelatedBoneDist, Dist);
                }

                // Check sibling bones (e.g., clavicles attached to upper spine)
                for (int32 SiblingIdx : Siblings)
                {
                    FTransform SiblingTM = MeshComp->GetBoneWorldTransform(SiblingIdx);
                    SiblingTM.Scale3D = FVector(1, 1, 1);
                    float Dist = (SiblingTM.Translation - SpinePos).Size();
                    MaxRelatedBoneDist = FMath::Max(MaxRelatedBoneDist, Dist);
                }

                // Expand radius to cover torso volume
                // Use a larger multiplier for spine since it's the torso "core"
                float MaxVertexDistance = MaxRelatedBoneDist * 2.5f;

                int32 VertexCountBefore = VerticesToFit.Num();

                // Include children's vertices (e.g., upper spine chain)
                // EXCLUDE neck - it should be its own body that absorbs head
                TArray<int32> AbsorbedChildren;
                for (int32 ChildIdx : Children)
                {
                    if (ChildIdx < InfluenceMap.Num())
                    {
                        // Check if this child is a neck bone - don't absorb it
                        FString ChildName = Bones[ChildIdx].Name;
                        std::transform(ChildName.begin(), ChildName.end(), ChildName.begin(), ::tolower);
                        bool bIsNeck = (ChildName.find("neck") != FString::npos) || (ChildName.find("head") != FString::npos);

                        if (bIsNeck)
                        {
                            UE_LOG("GenerateAllBodies: Spine '%s' - skipping child '%s' (neck/head should be separate body)",
                                Bone.Name.c_str(), Bones[ChildIdx].Name.c_str());
                            continue;  // Skip neck - let it be its own body
                        }

                        int32 ChildVertexCount = 0;
                        for (const FVector& Vert : InfluenceMap[ChildIdx].Vertices)
                        {
                            if ((Vert - SpinePos).Size() <= MaxVertexDistance)
                            {
                                VerticesToFit.Add(Vert);
                                ChildVertexCount++;
                            }
                        }

                        // If most of child's vertices were absorbed, mark as absorbed
                        if (ChildVertexCount > 0 && ChildVertexCount >= InfluenceMap[ChildIdx].Vertices.Num() * 0.7f)
                        {
                            if (!AbsorbedBones.Contains(ChildIdx))
                            {
                                AbsorbedBones.Add(ChildIdx);
                                AbsorbedChildren.Add(ChildIdx);
                            }
                        }
                    }
                }

                // Include siblings' vertices (e.g., clavicles, scapulae, shoulders for upper spine)
                TArray<int32> AbsorbedSiblings;
                for (int32 SiblingIdx : Siblings)
                {
                    if (SiblingIdx < InfluenceMap.Num())
                    {
                        FString SiblingName = Bones[SiblingIdx].Name;
                        std::transform(SiblingName.begin(), SiblingName.end(), SiblingName.begin(), ::tolower);

                        // Only include torso-related siblings (not arms/legs)
                        bool bIsTorsoSibling =
                            (SiblingName.find("clavicle") != FString::npos) ||
                            (SiblingName.find("scapula") != FString::npos) ||
                            (SiblingName.find("shoulder") != FString::npos) ||  // Added shoulder
                            (SiblingName.find("neck") != FString::npos) ||
                            (SiblingName.find("spine") != FString::npos) ||
                            (SiblingName.find("chest") != FString::npos);

                        if (bIsTorsoSibling)
                        {
                            int32 SiblingVertexCount = 0;
                            for (const FVector& Vert : InfluenceMap[SiblingIdx].Vertices)
                            {
                                if ((Vert - SpinePos).Size() <= MaxVertexDistance)
                                {
                                    VerticesToFit.Add(Vert);
                                    SiblingVertexCount++;
                                }
                            }

                            // If most of sibling's vertices were absorbed, mark as absorbed
                            if (SiblingVertexCount > 0 && SiblingVertexCount >= InfluenceMap[SiblingIdx].Vertices.Num() * 0.7f)
                            {
                                if (!AbsorbedBones.Contains(SiblingIdx))
                                {
                                    AbsorbedBones.Add(SiblingIdx);
                                    AbsorbedSiblings.Add(SiblingIdx);
                                }
                            }
                        }
                    }
                }

                UE_LOG("GenerateAllBodies: Spine '%s' - expanded from %d to %d vertices (torso integration, radius=%.1f)",
                    Bone.Name.c_str(), VertexCountBefore, VerticesToFit.Num(), MaxVertexDistance);
            }
            else
            {
                UE_LOG("GenerateAllBodies: Processing bone %s with %d influenced vertices",
                    Bone.Name.c_str(), Influence.Vertices.Num());
            }
        }

        // Create new BodySetup
        UBodySetup* NewBody = NewObject<UBodySetup>();
        if (!NewBody)
        {
            UE_LOG("GenerateAllBodies: Failed to create BodySetup for bone %s", Bone.Name.c_str());
            continue;
        }

        NewBody->BoneName = BoneName;
        NewBody->BodyType = ToBodySetupType(PrimitiveType);

        // Step 3: Calculate principal axis from vertex cloud
        FVector PrincipalAxis = CalculatePrincipalAxis(VerticesToFit);

        // Step 4: Fit minimal bounding primitive based on type
        FVector FittedCenter = FVector::Zero();
        FQuat   FittedRotation = FQuat::Identity();
        float   Radius = 0.0f;
        float   HalfHeight = 0.0f;
        FVector Extent = FVector::Zero();

        // Scale down the fitted dimensions for tighter collision
        // Physics bodies should be slightly smaller than the visual mesh for better simulation
        const float ColliderShrinkFactor = 0.8f;  // 80% of the fitted size

        switch (PrimitiveType)
        {
        case EPrimitiveType::Sphere:
            FitMinimalSphere(VerticesToFit, FittedCenter, Radius);
            Radius *= ColliderShrinkFactor;
            NewBody->SphereRadius = Radius;
            break;

        case EPrimitiveType::Box:
            FitMinimalBox(VerticesToFit, PrincipalAxis, FittedCenter, FittedRotation, Extent);
            NewBody->BoxExtent = Extent;
            NewBody->BoxExtent = Extent;
            break;

        case EPrimitiveType::Capsule:
            FitMinimalCapsule(VerticesToFit, PrincipalAxis, FittedCenter, FittedRotation, Radius, HalfHeight);
            Radius *= ColliderShrinkFactor;
            HalfHeight *= ColliderShrinkFactor;
            NewBody->CapsuleHalfHeight = HalfHeight;
            NewBody->SphereRadius = Radius;
            break;
        case EPrimitiveType::Convex:
            break;
        default:
            UE_LOG("GenerateAllBodies: Unknown primitive type for bone %s", Bone.Name.c_str());
            continue;
        }

        // Compute bone-local center and rotation (same rules as CreateBodyForBone)
        FVector LocalCenter;
        FQuat   LocalRotation;

        // Neck/Head should use fitted center from vertices to properly cover head mesh
        if (bIsNeck)
        {
            // Convert world-space fitted center to bone-local space
            FTransform BoneWorldTM = MeshComp->GetBoneWorldTransform(BoneIndex);
            BoneWorldTM.Scale3D = FVector(1, 1, 1);
            LocalCenter = BoneWorldTM.Inverse().TransformPosition(FittedCenter);
            // Convert world rotation to bone-local rotation
            LocalRotation = BoneWorldTM.Rotation.Inverse() * FittedRotation;

            UE_LOG("GenerateAllBodies: Neck '%s' - using vertex-fitted center (local: %.1f, %.1f, %.1f)",
                Bone.Name.c_str(), LocalCenter.X, LocalCenter.Y, LocalCenter.Z);
        }
        else
        {
            // Other bones use bone-local center
            CalculateBoneLocalShapeTransform(BoneIndex, Skeleton, MeshComp, LocalCenter, LocalRotation);
        }

        // Create final primitive: use bone-local center/rotation and vertex-fitted dimensions
        switch (PrimitiveType)
        {
        case EPrimitiveType::Sphere:
        {
            FSphereElem SphereElem(Radius);
            SphereElem.Center = LocalCenter;
            NewBody->AggGeom.SphereElems.Add(SphereElem);
            break;
        }
        case EPrimitiveType::Box:
        {
            // Extent는 half-extent (vertex fitting 결과),
            // BodySetup 생성자는 full size를 받으므로 *2
            FBoxElem BoxElem(Extent.X * 2.0f, Extent.Y * 2.0f, Extent.Z * 2.0f);
            BoxElem.Center = LocalCenter;
            BoxElem.Rotation = LocalRotation;
            NewBody->AggGeom.BoxElems.Add(BoxElem);
            break;
        }
        case EPrimitiveType::Capsule:
        {
            float CapsuleLength = HalfHeight * 2.0f; // FSphylElem은 HalfHeight가 아닌 full length를 받음
            FSphylElem CapsuleElem(Radius, CapsuleLength);
            CapsuleElem.Center = LocalCenter;
            CapsuleElem.Rotation = LocalRotation;
            NewBody->AggGeom.SphylElems.Add(CapsuleElem);
            break;
        }
        case EPrimitiveType::Convex:
            {
                FConvexElem ConvexElem;
                
                FTransform WorldTM = MeshComp->GetWorldTransform();
                WorldTM.Scale3D = FVector::One();
                
                FTransform BoneTM = MeshComp->GetBoneWorldTransform(BoneIndex);
                BoneTM.Scale3D = FVector::One();
                
                FTransform InvBoneTM = BoneTM.Inverse();                
                
                TArray<FVector> BoneLocalVertex;
                BoneLocalVertex.Reserve(VerticesToFit.Num());
                for (const FVector& Vertex : VerticesToFit)
                {
                    FVector WorldPos = WorldTM.TransformPosition(Vertex);
                    FVector BoneLocal = InvBoneTM.TransformVector(WorldPos);                    
                    BoneLocalVertex.Add(BoneLocal);
                }

                TArray<FVector> ShapeLocalVertex;
                ShapeLocalVertex.Reserve(VerticesToFit.Num());
                FQuat InvLocalRot = LocalRotation.Inverse();
                for (FVector& BoneLocal : BoneLocalVertex)
                {
                    FVector Centered = BoneLocal - LocalCenter;
                    FVector ShapeLocal = InvLocalRot.RotateVector(Centered);
                    ShapeLocalVertex.Add(ShapeLocal);
                }
                
                UE_LOG("Bone: %s, VerticesToFit: %d", Bone.Name.c_str(), VerticesToFit.Num());
                if (FPhysicsUtils::GenerateConvexHull(BoneLocalVertex, ConvexElem))
                {
                    ConvexElem.SetTransform(FTransform(LocalCenter, LocalRotation, FVector::One())); 
                
                    NewBody->AggGeom.ConvexElems.Add(ConvexElem);
                }
                else
                {
                    UE_LOG("GenerateAllBodies: Failed to generate convex hull for bone %s", Bone.Name.c_str());
                }
                if (NewBody->AggGeom.ConvexElems.Num() > 0)
                {
                    const FConvexElem& Elem = NewBody->AggGeom.ConvexElems.Last();
                    UE_LOG(">> Generated Convex: Verts=%d, Indices=%d", Elem.VertexData.Num(), Elem.IndexData.Num());
                
                    if (Elem.VertexData.Num() > 0)
                    {
                        // 첫 번째 정점 좌표 확인 (값이 터무니없이 크거나 NaN인지 체크)
                        FVector V = Elem.VertexData[0];
                        UE_LOG(">> First Vertex Local Pos: (%f, %f, %f)", V.X, V.Y, V.Z);
                    }
                }
                break;
            }
        }

        // Add the body to the physics asset
        PhysicsAsset->AddBodySetup(NewBody);
        CreatedCount++;
    }

    // Update the body index map
    PhysicsAsset->UpdateBodySetupIndexMap();

    UE_LOG("GenerateAllBodies: Completed vertex-driven generation - created %d bodies", CreatedCount);

    // Mark collision shapes dirty to visualize the newly created bodies
    ActiveState->bBoneLinesDirty = true;
    bCollisionShapesDirty = true;
}

void SPhysicsAssetEditorWindow::CreateConstraintForBone(int32 ChildBoneIndex)
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset || !ActiveState->CurrentMesh)
        return;

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();

    if (!PhysicsAsset || !Skeleton || !MeshComp)
        return;

    if (ChildBoneIndex < 0 || ChildBoneIndex >= Skeleton->Bones.Num())
        return;

    const FBone& Child = Skeleton->Bones[ChildBoneIndex];
    int32 ParentIndex = Child.ParentIndex;

    // Root bone cannot have a constraint
    if (ParentIndex < 0)
        return;

    const FBone& Parent = Skeleton->Bones[ParentIndex];

    // Build setup using common helper
    FTransform ChildWT = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
    FTransform ParentWT = MeshComp->GetBoneWorldTransform(ParentIndex);

    // 본 이름으로 BodySetup 찾아서 캡슐 정보 추출
    FVector ParentCapsuleCenter = FVector::Zero();
    float ParentCapsuleHalfHeight = 0.0f;
    float ParentCapsuleRadius = 1.0f;
    FQuat ParentCapsuleRotation = FQuat::Identity();
    FVector ChildCapsuleCenter = FVector::Zero();

    const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
    for (UBodySetup* Body : Bodies)
    {
        if (Body && Body->BoneName == FName(Parent.Name.c_str()))
        {
            if (!Body->AggGeom.SphylElems.IsEmpty())
            {
                const FSphylElem& Capsule = Body->AggGeom.SphylElems[0];
                ParentCapsuleCenter = Capsule.Center;
                ParentCapsuleHalfHeight = Capsule.Length * 0.5f; // FSphylElem.Length는 전체 길이
                ParentCapsuleRadius = Capsule.Radius;
                ParentCapsuleRotation = Capsule.Rotation;
            }
        }
        if (Body && Body->BoneName == FName(Child.Name.c_str()))
        {
            if (!Body->AggGeom.SphylElems.IsEmpty())
                ChildCapsuleCenter = Body->AggGeom.SphylElems[0].Center;
        }
    }

    // 컴포넌트 스케일 가져오기 (월드 트랜스폼을 로컬로 변환할 때 필요)
    FVector ComponentScale = MeshComp->GetWorldTransform().Scale3D;

    FConstraintSetup Setup;
    BuildConstraintSetup(
        FName(Parent.Name.c_str()),
        FName(Child.Name.c_str()),
        ParentWT,
        ChildWT,
        ParentCapsuleCenter,
        ParentCapsuleHalfHeight,
        ParentCapsuleRadius,
        ParentCapsuleRotation,
        ChildCapsuleCenter,
        ComponentScale,
        Setup
    );

    // Add to PhysicsAsset
    PhysicsAsset->GetConstraintSetupsMutable().Add(Setup);

    UE_LOG("CreateConstraintForBone: Added %s -> %s",
        Child.Name.c_str(), Parent.Name.c_str());
}

void SPhysicsAssetEditorWindow::GenerateAllConstraints()
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset || !ActiveState->CurrentMesh)
        return;

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();

    if (!PhysicsAsset || !Skeleton || !MeshComp)
        return;

    // Clear existing constraint setups
    PhysicsAsset->GetConstraintSetupsMutable().Empty();

    const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
    const TArray<FBone>& Bones = Skeleton->Bones;

    int32 CreatedCount = 0;

    // Only create constraints between bodies (not all bones)
    for (int32 ChildBodyIndex = 0; ChildBodyIndex < Bodies.Num(); ++ChildBodyIndex)
    {
        UBodySetup* ChildBody = Bodies[ChildBodyIndex];
        int32 ChildBoneIndex = BoneNameToIndex(ChildBody->BoneName);

        if (ChildBoneIndex < 0 || ChildBoneIndex >= Bones.Num())
            continue;

        const FBone& ChildBone = Bones[ChildBoneIndex];
        int32 ParentBoneIndex = ChildBone.ParentIndex;

        if (ParentBoneIndex < 0)
            continue;

        // Find the parent body (traverse up the bone hierarchy until we find a bone with a body)
        int32 ParentBodyIndex = -1;
        int32 CurrentBoneIndex = ParentBoneIndex;

        while (CurrentBoneIndex >= 0 && ParentBodyIndex < 0)
        {
            const FBone& CurrentBone = Bones[CurrentBoneIndex];
            FName CurrentBoneName = FName(CurrentBone.Name.c_str());

            // Check if this bone has a body
            for (int32 i = 0; i < Bodies.Num(); ++i)
            {
                if (Bodies[i]->BoneName == CurrentBoneName)
                {
                    ParentBodyIndex = i;
                    break;
                }
            }

            // Move up the hierarchy
            if (ParentBodyIndex < 0)
                CurrentBoneIndex = CurrentBone.ParentIndex;
        }

        // If we found a parent body, create a constraint
        if (ParentBodyIndex >= 0)
        {
            CreateConstraintBetweenBodies(ParentBodyIndex, ChildBodyIndex);
            CreatedCount++;
        }
    }

    UE_LOG("GenerateAllConstraints: Created %d constraints between %d bodies", CreatedCount, Bodies.Num());
}

void SPhysicsAssetEditorWindow::CreateConstraintBetweenBodies(int ParentBodyIndex, int ChildBodyIndex)
{
    if (!ActiveState || !ActiveState->CurrentPhysicsAsset || !ActiveState->CurrentMesh)
        return;

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    USkeletalMeshComponent* Mesh = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();

    if (!PhysicsAsset || !Mesh || !Skeleton)
        return;

    if (ParentBodyIndex < 0 || ChildBodyIndex < 0)
        return;

    const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
    if (ParentBodyIndex >= Bodies.Num() || ChildBodyIndex >= Bodies.Num())
        return;

    UBodySetup* ParentBody = Bodies[ParentBodyIndex];
    UBodySetup* ChildBody = Bodies[ChildBodyIndex];

    int32 ParentBoneIndex = BoneNameToIndex(ParentBody->BoneName);
    int32 ChildBoneIndex = BoneNameToIndex(ChildBody->BoneName);

    if (ParentBoneIndex < 0 || ChildBoneIndex < 0)
        return;

    FTransform ParentWT = Mesh->GetBoneWorldTransform(ParentBoneIndex);
    FTransform ChildWT = Mesh->GetBoneWorldTransform(ChildBoneIndex);

    // 캡슐 정보 추출
    FVector ParentCapsuleCenter = FVector::Zero();
    float ParentCapsuleHalfHeight = 0.0f;
    float ParentCapsuleRadius = 1.0f;
    FQuat ParentCapsuleRotation = FQuat::Identity();
    FVector ChildCapsuleCenter = FVector::Zero();

    if (!ParentBody->AggGeom.SphylElems.IsEmpty())
    {
        const FSphylElem& Capsule = ParentBody->AggGeom.SphylElems[0];
        ParentCapsuleCenter = Capsule.Center;
        ParentCapsuleHalfHeight = Capsule.Length * 0.5f; // FSphylElem.Length는 전체 길이
        ParentCapsuleRadius = Capsule.Radius;
        ParentCapsuleRotation = Capsule.Rotation;
    }
    if (!ChildBody->AggGeom.SphylElems.IsEmpty())
        ChildCapsuleCenter = ChildBody->AggGeom.SphylElems[0].Center;

    // 컴포넌트 스케일 가져오기 (월드 트랜스폼을 로컬로 변환할 때 필요)
    FVector ComponentScale = Mesh->GetWorldTransform().Scale3D;

    FConstraintSetup Setup;
    BuildConstraintSetup(
        ParentBody->BoneName,
        ChildBody->BoneName,
        ParentWT,
        ChildWT,
        ParentCapsuleCenter,
        ParentCapsuleHalfHeight,
        ParentCapsuleRadius,
        ParentCapsuleRotation,
        ChildCapsuleCenter,
        ComponentScale,
        Setup
    );

    PhysicsAsset->GetConstraintSetupsMutable().Add(Setup);

    UE_LOG("CreateConstraintBetweenBodies: Added constraint [%s -> %s]",
        ChildBody->BoneName.ToString().c_str(),
        ParentBody->BoneName.ToString().c_str());
}

void SPhysicsAssetEditorWindow::BuildConstraintSetup(const FName& ParentBoneName, const FName& ChildBoneName, const FTransform& ParentWT, const FTransform& ChildWT, const FVector& ParentCapsuleCenter, float ParentCapsuleHalfHeight, float ParentCapsuleRadius, const FQuat& ParentCapsuleRotation, const FVector& ChildCapsuleCenter, const FVector& ComponentScale, FConstraintSetup& OutSetup)
{
    OutSetup = FConstraintSetup(); // reset

    // Basic identifiers
    OutSetup.JointName = ChildBoneName;
    OutSetup.ConstraintBone1 = ChildBoneName;
    OutSetup.ConstraintBone2 = ParentBoneName;

    // Frames
    FTransform PWT = ParentWT;
    FTransform CWT = ChildWT;

    PWT.Scale3D = FVector(1, 1, 1);
    CWT.Scale3D = FVector(1, 1, 1);

    // ========== 본 방향 기반 축 계산 ==========
    // Parent → Child 방향 = 본의 길이 방향 = Twist 축 (PhysX X축)
    FVector BoneDirection = (CWT.Translation - PWT.Translation).GetSafeNormal();

    // 본 방향이 거의 0인 경우 (같은 위치) 기본 축 사용
    if (BoneDirection.IsZero())
    {
        BoneDirection = FVector(1, 0, 0);
    }

    FVector WorldPriAxis = BoneDirection;

    // SecAxis = PriAxis에 수직인 축 계산
    // 월드 Up 벡터와 외적하여 수직 축 생성
    FVector WorldUp = FVector(0, 0, 1);
    FVector WorldSecAxis = FVector::Cross(WorldPriAxis, WorldUp).GetSafeNormal();

    // PriAxis가 WorldUp과 거의 평행한 경우 대체 축 사용
    if (WorldSecAxis.IsZero())
    {
        FVector WorldRight = FVector(0, 1, 0);
        WorldSecAxis = FVector::Cross(WorldPriAxis, WorldRight).GetSafeNormal();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Frame 위치 계산 (월드 트랜스폼 기반, 컴포넌트 스케일로 나눠서 로컬화)
    // ═══════════════════════════════════════════════════════════════════════════
    // 핵심 수정: Parent 캡슐의 끝단(자식 방향)에서 약간 안쪽 지점에 조인트 위치
    // 기존: 캡슐 중심에 연결 → 관절 접힐 때 스킨 부자연스러움
    // 수정: 캡슐 끝단 안쪽에 연결 → 자연스러운 관절 움직임
    // ═══════════════════════════════════════════════════════════════════════════

    // Child frame: Child 캡슐 중심에서 Child 본 원점(조인트 위치)으로의 오프셋
    // Frame1 (Child): Child 본-로컬 좌표계로 월드 축 변환
    OutSetup.Frame1.Pos = -ChildCapsuleCenter;
    OutSetup.Frame1.PriAxis = CWT.Rotation.Inverse().RotateVector(WorldPriAxis);
    OutSetup.Frame1.SecAxis = CWT.Rotation.Inverse().RotateVector(WorldSecAxis);

    // Parent frame: 월드 좌표에서 Child 본의 Parent 본 기준 상대 위치 계산
    // 월드 좌표 차이를 Parent 본 로컬로 변환 후 컴포넌트 스케일로 나눔
    FVector ChildPosWorld = CWT.Translation;
    FVector ParentPosWorld = PWT.Translation;

    // Parent 본 로컬 좌표계에서 Child 본의 위치 (월드 스케일)
    FVector ChildInParentWorld = PWT.Rotation.Inverse().RotateVector(ChildPosWorld - ParentPosWorld);

    // 컴포넌트 스케일로 나눠서 본 로컬 스케일로 변환
    // (월드 좌표는 컴포넌트 스케일이 적용된 상태이므로)
    FVector ChildInParentLocal = FVector(
        ChildInParentWorld.X / ComponentScale.X,
        ChildInParentWorld.Y / ComponentScale.Y,
        ChildInParentWorld.Z / ComponentScale.Z
    );

    // ═══════════════════════════════════════════════════════════════════════════
    // Frame2 (Parent) 위치 계산
    //
    // 언리얼 방식: Frame2.Pos = ChildInParentLocal - ParentCapsuleCenter
    // - ChildInParentLocal: Parent 본 로컬 좌표계에서 Child 본의 원점 위치
    // - ParentCapsuleCenter: Parent 본 로컬 좌표계에서 Parent 캡슐의 중심 위치
    // - 결과: Parent 캡슐 로컬 좌표계에서 조인트(Child 본 원점) 위치
    //
    // Frame2.Pos = 캡슐 로컬 좌표계에서 조인트 위치
    // ═══════════════════════════════════════════════════════════════════════════

    OutSetup.Frame2.Pos = ChildInParentLocal - ParentCapsuleCenter;
    OutSetup.Frame2.PriAxis = PWT.Rotation.Inverse().RotateVector(WorldPriAxis);
    OutSetup.Frame2.SecAxis = PWT.Rotation.Inverse().RotateVector(WorldSecAxis);

    // Angular limits
    OutSetup.Profile.Swing1Motion = EJointMotion::Limited;
    OutSetup.Profile.Swing2Motion = EJointMotion::Limited;
    OutSetup.Profile.TwistMotion = EJointMotion::Limited;

    OutSetup.Profile.Swing1LimitsAngle = 40.f;
    OutSetup.Profile.Swing2LimitsAngle = 40.f;
    OutSetup.Profile.TwistLimit = 25.f;

    // Lock linear movement
    OutSetup.Profile.LinearMotionX = EJointMotion::Locked;
    OutSetup.Profile.LinearMotionY = EJointMotion::Locked;
    OutSetup.Profile.LinearMotionZ = EJointMotion::Locked;

    // Disable collision between connected bodies
    OutSetup.Profile.bDisableCollision = true;

    // Drive disabled
    OutSetup.Profile.bEnableDrive = false;
}

int32 SPhysicsAssetEditorWindow::FindFirstChildBone(int32 BoneIndex, const FSkeleton* Skeleton) const
{
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
        return -1;

    const TArray<FBone>& Bones = Skeleton->Bones;
    for (int32 i = 0; i < Bones.Num(); ++i)
    {
        if (Bones[i].ParentIndex == BoneIndex)
        {
            return i;   // first child
        }
    }

    return -1;  // no child
}

void SPhysicsAssetEditorWindow::GetAllChildBones(int32 BoneIndex, const FSkeleton* Skeleton, TArray<int32>& OutChildren) const
{
    OutChildren.Empty();

    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
        return;

    const TArray<FBone>& Bones = Skeleton->Bones;
    for (int32 i = 0; i < Bones.Num(); ++i)
    {
        if (Bones[i].ParentIndex == BoneIndex)
        {
            OutChildren.Add(i);
        }
    }
}

int32 SPhysicsAssetEditorWindow::BoneNameToIndex(const FName& BoneName) const
{
    if (!ActiveState || !ActiveState->CurrentMesh)
        return -1;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton)
        return -1;

    const TArray<FBone>& Bones = Skeleton->Bones;

    for (int32 i = 0; i < Bones.Num(); ++i)
    {
        if (Bones[i].Name == BoneName.ToString())
            return i;
    }

    return -1; // not found
}

bool SPhysicsAssetEditorWindow::IsDescendantOf(int32 ChildIdx, int32 RootIdx, const TArray<FBone>& Bones) const
{
    int32 Current = ChildIdx;

    while (Current >= 0 && Current < Bones.Num())
    {
        int32 Parent = Bones[Current].ParentIndex;
        if (Parent == RootIdx)
            return true;
        Current = Parent;
    }
    return false;
}

float SPhysicsAssetEditorWindow::ComputeLimbChainLength(int32 RootBoneIndex, const FSkeleton* Skeleton, USkeletalMeshComponent* MeshComp) const
{
    if (!Skeleton || !MeshComp)
        return 0.f;

    const TArray<FBone>& Bones = Skeleton->Bones;
    if (RootBoneIndex < 0 || RootBoneIndex >= Bones.Num())
        return 0.f;

    // Use world transform for correct scale/units
    FTransform RootWorldTM = MeshComp->GetBoneWorldTransform(RootBoneIndex);
    RootWorldTM.Scale3D = FVector(1, 1, 1);
    FVector RootPos = RootWorldTM.Translation;

    float MaxDist = 0.f;

    // Find the maximum distance from root to any descendant (e.g., finger tips)
    for (int32 i = 0; i < Bones.Num(); ++i)
    {
        if (IsDescendantOf(i, RootBoneIndex, Bones))
        {
            FTransform ChildWorldTM = MeshComp->GetBoneWorldTransform(i);
            ChildWorldTM.Scale3D = FVector(1, 1, 1);
            FVector ChildPos = ChildWorldTM.Translation;

            float Dist = (ChildPos - RootPos).Size();
            MaxDist = FMath::Max(MaxDist, Dist);
        }
    }

    return MaxDist;
}

FVector SPhysicsAssetEditorWindow::ComputeLimbCentroid(int32 RootBoneIndex, const FSkeleton* Skeleton, USkeletalMeshComponent* MeshComp) const
{
    if (!Skeleton || !MeshComp)
        return FVector::Zero();

    const TArray<FBone>& Bones = Skeleton->Bones;
    if (RootBoneIndex < 0 || RootBoneIndex >= Bones.Num())
        return FVector::Zero();

    // Get root position
    FTransform RootWorldTM = MeshComp->GetBoneWorldTransform(RootBoneIndex);
    RootWorldTM.Scale3D = FVector(1, 1, 1);
    FVector RootPos = RootWorldTM.Translation;

    // Find the furthest descendant bone to determine direction
    float MaxDist = 0.f;
    FVector FurthestPos = RootPos;

    for (int32 i = 0; i < Bones.Num(); ++i)
    {
        if (IsDescendantOf(i, RootBoneIndex, Bones))
        {
            FTransform ChildWorldTM = MeshComp->GetBoneWorldTransform(i);
            ChildWorldTM.Scale3D = FVector(1, 1, 1);
            FVector ChildPos = ChildWorldTM.Translation;

            float Dist = (ChildPos - RootPos).Size();
            if (Dist > MaxDist)
            {
                MaxDist = Dist;
                FurthestPos = ChildPos;
            }
        }
    }

    // Center the collider: root position + half distance toward furthest descendant
    // Apply 5% padding to match the dimension calculation
    const float PaddingMultiplier = 1.05f;
    FVector Direction = (FurthestPos - RootPos).GetSafeNormal();
    return RootPos + Direction * (MaxDist * PaddingMultiplier * 0.5f);
}

void SPhysicsAssetEditorWindow::CalculateBoneLocalShapeTransform(int32 BoneIndex, const FSkeleton* Skeleton, USkeletalMeshComponent* MeshComp, FVector& OutLocalCenter, FQuat& OutLocalRotation)
{
    OutLocalCenter = FVector::Zero();
    OutLocalRotation = FQuat::Identity();

    if (!Skeleton || !MeshComp)
        return;

    // Find first child bone
    int32 FirstChildIndex = FindFirstChildBone(BoneIndex, Skeleton);

    // Get bone world transform (remove scale)
    FTransform BoneWorldTM = MeshComp->GetBoneWorldTransform(BoneIndex);
    BoneWorldTM.Scale3D = FVector(1, 1, 1);
    UE_LOG("EDITOR Bone %d World: Pos=(%.2f,%.2f,%.2f)", BoneIndex, BoneWorldTM.Translation.X, BoneWorldTM.Translation.Y,BoneWorldTM.Translation.Z);

    FVector BoneWorldPos = BoneWorldTM.Translation;

    // Child world pos (fallback for leaf bones)
    FVector ChildWorldPos = BoneWorldPos;
    if (FirstChildIndex >= 0)
    {
        FTransform ChildWorldTM = MeshComp->GetBoneWorldTransform(FirstChildIndex);
        ChildWorldTM.Scale3D = FVector(1, 1, 1);
        ChildWorldPos = ChildWorldTM.Translation;
    }

    // 1) LocalCenter ---------------------------------
    // ═══════════════════════════════════════════════════════════════════════════
    // 캡슐 중심 위치 계산 전략:
    // 기존: 본 시작점과 자식 본 사이의 중간점 → 관절 접힘 시 빈 공간 발생
    // 수정: 본 시작점에서 본 방향으로 HalfHeight만큼만 이동
    //       → 캡슐 시작점이 본 원점(조인트 위치)과 일치하여 자연스러운 접힘
    //
    // 시각화:
    // 기존: Bone──────[====●====]──────Child (●=center, 중간점)
    // 수정: Bone[==●==]──────────────Child (●=center, 본 원점 기준)
    // ═══════════════════════════════════════════════════════════════════════════
    const TArray<FBone>& Bones = Skeleton->Bones;
    FString BoneName = Bones[BoneIndex].Name;
    std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

    bool bIsHandOrFoot =
        (BoneName.find("hand") != FString::npos) ||
        (BoneName.find("foot") != FString::npos);

    bool bIsPelvis =
        (BoneName.find("pelvis") != FString::npos) ||
        (BoneName.find("hips") != FString::npos);

    FVector WorldCenter;
    if (bIsHandOrFoot && FirstChildIndex >= 0)
    {
        // For hand/foot: use centroid of all descendant bones for proper centering
        WorldCenter = ComputeLimbCentroid(BoneIndex, Skeleton, MeshComp);
    }
    else if (bIsPelvis)
    {
        // For pelvis/hips: keep center at pelvis position
        WorldCenter = BoneWorldPos;
    }
    else
    {
        // ═══════════════════════════════════════════════════════════════════════
        // 캡슐 배치 전략: 본 원점에서 캡슐 시작점이 시작하도록 배치
        //
        // 문제: 캡슐 중심이 본 중간점에 있으면 관절 접힘 시 빈 공간 발생
        // 해결: 캡슐 시작점(중심 - HalfHeight)이 본 원점과 일치하도록 배치
        //
        // 시각화:
        // 기존: Joint───[====●====]───Child (캡슐 중심이 본 중간점)
        //       접힘 시 빈 공간 ↗
        //
        // 수정: Joint(==●==)────────Child (캡슐 시작점이 본 원점)
        //       접힘 시 캡슐이 조인트 위치에서 바로 시작 → 빈 공간 최소화
        //
        // 계산: 캡슐 길이(Length) = 본 길이 - 2*Radius (CalculateBodyDimensions에서)
        //       캡슐 중심 = 본 원점 + 방향 * (Length/2 + Radius)
        //       → 캡슐 시작점 = 중심 - HalfHeight = 본 원점 + Radius (반구 끝이 본 원점)
        //
        // 더 간단한 방법: 본 원점에서 Radius만큼 떨어진 곳에 캡슐 시작
        // ═══════════════════════════════════════════════════════════════════════
        FVector BoneDirection = (ChildWorldPos - BoneWorldPos).GetSafeNormal();
        float BoneLength = (ChildWorldPos - BoneWorldPos).Size();

        // 캡슐 파라미터 추정 (CalculateBodyDimensions와 동일한 로직)
        // 캡슐이 약간 겹치도록 길이를 10% 늘림
        const float OverlapFactor = 1.1f;
        float EffectiveBoneLength = BoneLength * OverlapFactor;

        const float MinRadius = 0.01f;
        float EstimatedRadius = FMath::Max(EffectiveBoneLength * 0.25f, MinRadius);
        float CylinderLength = EffectiveBoneLength - (EstimatedRadius * 2.0f);
        CylinderLength = FMath::Max(CylinderLength, 0.01f);
        float HalfHeight = CylinderLength * 0.5f;

        // 캡슐 시작점(반구 시작)이 본 원점과 일치하도록 중심 위치 계산
        // 캡슐 시작점 = Center - HalfHeight - Radius
        // 캡슐 시작점 = 본 원점 원할 경우:
        // Center = 본 원점 + HalfHeight + Radius
        float CenterOffset = HalfHeight + EstimatedRadius;
        WorldCenter = BoneWorldPos + BoneDirection * CenterOffset;
    }

    FVector WorldOffset = WorldCenter - BoneWorldPos;

    FQuat InvRot = BoneWorldTM.Rotation.Inverse();
    OutLocalCenter = InvRot.RotateVector(WorldOffset);

    // 2) LocalRotation --------------------------------
    // Align collider Z-axis with bone direction
    FVector WorldDir = (ChildWorldPos - BoneWorldPos).GetSafeNormal();
    if (WorldDir.IsZero())
    {
        FVector LocalDir = InvRot.RotateVector(WorldDir);
        OutLocalRotation = FQuat::FindBetween(FVector(0, 0, 1), LocalDir);
    }
}

void SPhysicsAssetEditorWindow::RecreateBodyPrimitive(UBodySetup* BodySetup, EPrimitiveType NewPrimitiveType)
{
    if (!BodySetup || !ActiveState || !ActiveState->CurrentMesh)
        return;

    USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp)
        return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton)
        return;

    // Find bone index for this body
    int32 BoneIndex = BoneNameToIndex(BodySetup->BoneName);
    if (BoneIndex < 0)
        return;

    // Calculate center & rotation using the same logic as CreateBodyForBone
    FVector LocalCenter;
    FQuat LocalRotation;
    CalculateBoneLocalShapeTransform(BoneIndex, Skeleton, MeshComp, LocalCenter, LocalRotation);

    // Calculate dimensions using the same logic as CreateBodyForBone
    float Radius, HalfHeight;
    FVector Extent;
    CalculateBodyDimensions(BoneIndex, Skeleton, MeshComp, NewPrimitiveType,
                            Radius, HalfHeight, Extent);

    // Update the property values in BodySetup
    BodySetup->SphereRadius = Radius;
    BodySetup->BoxExtent = Extent;
    BodySetup->CapsuleHalfHeight = HalfHeight;

    // Clear all existing primitives
    BodySetup->AggGeom.SphereElems.Empty();
    BodySetup->AggGeom.BoxElems.Empty();
    BodySetup->AggGeom.SphylElems.Empty();

    // Create new primitive based on type
    switch (NewPrimitiveType)
    {
    case EPrimitiveType::Sphere:
    {
        FSphereElem SphereElem(Radius);
        SphereElem.Center = LocalCenter;
        BodySetup->AggGeom.SphereElems.Add(SphereElem);
        break;
    }
    case EPrimitiveType::Box:
    {
        FBoxElem BoxElem(Extent.X * 2.f, Extent.Y * 2.f, Extent.Z * 2.f);
        BoxElem.Center = LocalCenter;
        BoxElem.Rotation = LocalRotation;
        BodySetup->AggGeom.BoxElems.Add(BoxElem);
        break;
    }
    case EPrimitiveType::Capsule:
    {
        FSphylElem CapsuleElem(Radius, HalfHeight * 2.f);
        CapsuleElem.Center = LocalCenter;
        CapsuleElem.Rotation = LocalRotation;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
        break;
    }
    default:
        break;
    }

    UE_LOG("RecreateBodyPrimitive: Recreated primitive for bone '%s' as type %d",
        BodySetup->BoneName.ToString().c_str(), (int)NewPrimitiveType);
}

bool SPhysicsAssetEditorWindow::ShouldCreateBodyForBone(int32 BoneIndex, const FSkeleton* Skeleton) const
{
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return false;

    const TArray<FBone>& Bones = Skeleton->Bones;
    const FBone& Bone = Bones[BoneIndex];

    // 1. Check for special bone name patterns to exclude
    FString BoneName = Bone.Name;
    std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

    // Exclude IK, twist, marker, and other utility bones
    if (BoneName.find("ik") != FString::npos ||
        BoneName.find("twist") != FString::npos ||
        BoneName.find("_end") != FString::npos ||
        BoneName.find("marker") != FString::npos ||
        BoneName.find("socket") != FString::npos ||
        BoneName.find("root") == 0)  // Skip root bone
    {
        return false;
    }

    // Exclude finger/toe bones by name pattern
    // These should be covered by parent hand/foot body
    if (BoneName.find("thumb") != FString::npos ||
        BoneName.find("index") != FString::npos ||
        BoneName.find("middle") != FString::npos ||
        BoneName.find("ring") != FString::npos ||
        BoneName.find("pinky") != FString::npos ||
        BoneName.find("toe") != FString::npos)
    {
        UE_LOG("ShouldCreateBodyForBone: Excluding finger/toe bone '%s'", Bone.Name.c_str());
        return false;
    }

    // 2. Exclude leaf bones (finger tips, toe tips, etc.)
    // A leaf bone has no children
    bool bHasChildren = false;
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        if (Bones[i].ParentIndex == BoneIndex)
        {
            bHasChildren = true;
            break;
        }
    }

    if (!bHasChildren)
    {
        // This is a leaf bone - exclude it
        UE_LOG("ShouldCreateBodyForBone: Excluding leaf bone '%s'", Bone.Name.c_str());
        return false;
    }

    // 3. Exclude very short bones (detail/helper bones)
    // Special handling: hand/foot are allowed even if short (they use chain length)
    bool bIsHandOrFoot =
        (BoneName.find("hand") != FString::npos) ||
        (BoneName.find("foot") != FString::npos);

    int32 FirstChildIndex = -1;
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        if (Bones[i].ParentIndex == BoneIndex)
        {
            FirstChildIndex = i;
            break;
        }
    }

    if (FirstChildIndex >= 0)
    {
        FVector BonePos = FVector(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);
        FVector ChildPos = FVector(
            Bones[FirstChildIndex].BindPose.M[3][0],
            Bones[FirstChildIndex].BindPose.M[3][1],
            Bones[FirstChildIndex].BindPose.M[3][2]
        );

        float BoneLength = (ChildPos - BonePos).Size();

        // Exclude bones shorter than 0.01 units (1cm)
        // BUT allow hand/foot even if short - they will use limb chain length
        if (!bIsHandOrFoot && BoneLength < 0.01f)
        {
            UE_LOG("ShouldCreateBodyForBone: Excluding short bone '%s' (length: %.2f)", Bone.Name.c_str(), BoneLength);
            return false;
        }
    }

    // Passed all filters - create body for this bone
    UE_LOG("ShouldCreateBodyForBone: Creating body for '%s'", Bone.Name.c_str());
    return true;
}

void SPhysicsAssetEditorWindow::CalculateBodyDimensions(int32 BoneIndex, const FSkeleton* Skeleton, USkeletalMeshComponent* MeshComp, 
        EPrimitiveType PrimitiveType, float& OutRadius, float& OutHalfHeight, FVector& OutExtent) const
{
    if (!Skeleton || !MeshComp)
    {
        // Default fallback dimensions
        OutRadius = 0.f;
        OutHalfHeight = 0.f;
        OutExtent = FVector::Zero();
        return;
    }

    // Find the first child bone
    int32 FirstChildIndex = FindFirstChildBone(BoneIndex, Skeleton);

    // Get world space bone transforms
    FTransform BoneWorldTM = MeshComp->GetBoneWorldTransform(BoneIndex);
    BoneWorldTM.Scale3D = FVector(1, 1, 1);
    FVector BoneWorldPos = BoneWorldTM.Translation;

    FVector ChildWorldPos = BoneWorldPos;
    if (FirstChildIndex >= 0)
    {
        FTransform ChildWorldTM = MeshComp->GetBoneWorldTransform(FirstChildIndex);
        ChildWorldTM.Scale3D = FVector(1, 1, 1);
        ChildWorldPos = ChildWorldTM.Translation;
    }

    // Calculate bone length
    float BoneLength = (ChildWorldPos - BoneWorldPos).Size();

    // Check for special bone types that need custom dimension calculation
    const TArray<FBone>& Bones = Skeleton->Bones;
    if (BoneIndex >= 0 && BoneIndex < Bones.Num())
    {
        FString BoneName = Bones[BoneIndex].Name;
        std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

        bool bIsHandOrFoot =
            (BoneName.find("hand") != FString::npos) ||
            (BoneName.find("foot") != FString::npos);

        bool bIsPelvis =
            (BoneName.find("pelvis") != FString::npos) ||
            (BoneName.find("hips") != FString::npos);

        if (bIsHandOrFoot)
        {
            // Use chain length from hand/foot root to furthest descendant (finger/toe tips)
            // Add 10% padding to ensure proper coverage
            const float PaddingMultiplier = 1.1f;
            float ChainLength = ComputeLimbChainLength(BoneIndex, Skeleton, MeshComp);
            if (ChainLength > 0.01f)
            {
                BoneLength = ChainLength * PaddingMultiplier;
                UE_LOG("CalculateBodyDimensions: Using limb chain length %.2f (with %.0f%% padding) for '%s'",
                    BoneLength, (PaddingMultiplier - 1.0f) * 100.0f, Bones[BoneIndex].Name.c_str());
            }
        }
        else if (bIsPelvis)
        {
            // For pelvis/hips: find max distance to any direct child
            TArray<int32> Children;
            GetAllChildBones(BoneIndex, Skeleton, Children);

            float MaxChildDist = 0.f;
            for (int32 ChildIdx : Children)
            {
                FTransform ChildTM = MeshComp->GetBoneWorldTransform(ChildIdx);
                ChildTM.Scale3D = FVector(1, 1, 1);
                float Dist = (ChildTM.Translation - BoneWorldPos).Size();
                MaxChildDist = FMath::Max(MaxChildDist, Dist);
            }

            if (MaxChildDist > 0.01f)
            {
                BoneLength = MaxChildDist;
                UE_LOG("CalculateBodyDimensions: Using max child distance %.2f for pelvis '%s'",
                    BoneLength, Bones[BoneIndex].Name.c_str());
            }
        }
    }

    // Estimate scale factor from average skeleton bone lengths to determine appropriate defaults
    // We sample multiple bones to get a better estimate of the overall model scale
    float AvgBoneLength = 0.0f;
    int32 SampleCount = 0;
    for (int32 i = 0; i < Skeleton->Bones.Num() && SampleCount < 10; ++i)
    {
        int32 ChildIdx = FindFirstChildBone(i, Skeleton);
        if (ChildIdx >= 0)
        {
            FTransform ParentTM = MeshComp->GetBoneWorldTransform(i);
            FTransform ChildTM = MeshComp->GetBoneWorldTransform(ChildIdx);
            ParentTM.Scale3D = FVector(1, 1, 1);
            ChildTM.Scale3D = FVector(1, 1, 1);
            float Length = (ChildTM.Translation - ParentTM.Translation).Size();
            if (Length > 0.001f)  // Exclude very small bones
            {
                AvgBoneLength += Length;
                SampleCount++;
            }
        }
    }
    if (SampleCount > 0)
    {
        AvgBoneLength /= SampleCount;
    }

    // Determine scale factor based on average bone length
    // If average bone length is very small (< 0.2), we're dealing with a small-scale model (e.g., FBX with cm units)
    float ScaleFactor = 1.0f;
    if (AvgBoneLength > 0.0f && AvgBoneLength < 0.2f)
    {
        ScaleFactor = 0.01f;
    }

    const float DefaultLeafLength = 5.0f * ScaleFactor;
    const float MinRadius = 1.0f * ScaleFactor;
    const float MinCylinderLength = 1.0f * ScaleFactor;

    if (FirstChildIndex < 0)
    {
        BoneLength = DefaultLeafLength;
    }

    // Calculate dimensions based on primitive type
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
    {
        float Radius = BoneLength * 0.5f;
        if (FirstChildIndex < 0)
            Radius = DefaultLeafLength * 0.5f;

        // Special handling for pelvis: BoneLength is max child distance from pelvis
        // But center is averaged, so we need full distance as radius
        if (BoneIndex >= 0 && BoneIndex < Bones.Num())
        {
            FString BoneName = Bones[BoneIndex].Name;
            std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

            bool bIsPelvis =
                (BoneName.find("pelvis") != FString::npos) ||
                (BoneName.find("hips") != FString::npos);

            if (bIsPelvis)
            {
                // For pelvis, use full BoneLength as radius to reach all children
                Radius = BoneLength;
            }
        }

        OutRadius = Radius;
        OutHalfHeight = 0.0f;
        OutExtent = FVector(Radius, Radius, Radius);
        break;
    }
    case EPrimitiveType::Box:
    {
        float HalfX = BoneLength * 0.2f;
        float HalfY = BoneLength * 0.2f;
        float HalfZ = BoneLength * 0.5f;

        // Special handling for pelvis: use full length for Z-axis
        if (BoneIndex >= 0 && BoneIndex < Bones.Num())
        {
            FString BoneName = Bones[BoneIndex].Name;
            std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

            bool bIsPelvis =
                (BoneName.find("pelvis") != FString::npos) ||
                (BoneName.find("hips") != FString::npos);

            if (bIsPelvis)
            {
                // For pelvis: calculate AABB from pelvis + all children
                // Center was calculated as AABB center in CalculateBoneLocalShapeTransform
                TArray<int32> Children;
                GetAllChildBones(BoneIndex, Skeleton, Children);

                FVector MinBound = BoneWorldPos;
                FVector MaxBound = BoneWorldPos;

                for (int32 ChildIdx : Children)
                {
                    FTransform ChildTM = MeshComp->GetBoneWorldTransform(ChildIdx);
                    ChildTM.Scale3D = FVector(1, 1, 1);
                    FVector ChildPos = ChildTM.Translation;

                    MinBound.X = FMath::Min(MinBound.X, ChildPos.X);
                    MinBound.Y = FMath::Min(MinBound.Y, ChildPos.Y);
                    MinBound.Z = FMath::Min(MinBound.Z, ChildPos.Z);

                    MaxBound.X = FMath::Max(MaxBound.X, ChildPos.X);
                    MaxBound.Y = FMath::Max(MaxBound.Y, ChildPos.Y);
                    MaxBound.Z = FMath::Max(MaxBound.Z, ChildPos.Z);
                }

                // AABB half-extent in world space
                FVector WorldHalfExtent = (MaxBound - MinBound) * 0.5f * 1.1f;  // 10% padding

                // Convert to bone local space
                FQuat InvBoneRot = BoneWorldTM.Rotation.Inverse();
                FVector LocalHalfExtent = InvBoneRot.RotateVector(WorldHalfExtent);

                HalfX = FMath::Abs(LocalHalfExtent.X);
                HalfY = FMath::Abs(LocalHalfExtent.Y);
                HalfZ = FMath::Abs(LocalHalfExtent.Z);
            }
        }

        OutRadius = 0.0f;
        OutHalfHeight = 0.0f;
        OutExtent = FVector(HalfX, HalfY, HalfZ);
        break;
    }
    case EPrimitiveType::Capsule:
    {
        // 캡슐이 약간 겹치도록 길이를 10% 늘림
        // 이렇게 하면 관절이 접힐 때 스킨이 자연스럽게 보임
        const float OverlapFactor = 1.1f;  // 10% 오버랩
        float EffectiveBoneLength = BoneLength * OverlapFactor;

        float Radius = FMath::Max(EffectiveBoneLength * 0.25f, MinRadius);
        float CylinderLength = EffectiveBoneLength - (Radius * 2.0f);
        CylinderLength = FMath::Max(CylinderLength, MinCylinderLength);

        // Special handling for pelvis: use full length for cylinder
        if (BoneIndex >= 0 && BoneIndex < Bones.Num())
        {
            FString BoneName = Bones[BoneIndex].Name;
            std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

            bool bIsPelvis =
                (BoneName.find("pelvis") != FString::npos) ||
                (BoneName.find("hips") != FString::npos);

            if (bIsPelvis)
            {
                // For pelvis, use full BoneLength to cover all children
                Radius = BoneLength * 0.3f;  // Wider radius for legs
                CylinderLength = BoneLength * 2.0f;  // Full length coverage
            }
        }

        OutRadius = Radius;
        OutHalfHeight = CylinderLength * 0.5f;
        OutExtent = FVector(Radius, Radius, OutHalfHeight);
        break;
    }
    }
}

// =========================================
// Vertex-Driven Body Generation (UE Style)
// =========================================
void SPhysicsAssetEditorWindow::BuildBoneVertexInfluenceMap(const FSkeletalMeshData* MeshData, TArray<FBoneVertexInfluence>& OutInfluenceMap, float MinWeightThreshold) const
{
    if (!MeshData)
    {
        UE_LOG("BuildBoneVertexInfluenceMap: MeshData is null");
        return;
    }

    const FSkeleton& Skeleton = MeshData->Skeleton;
    const TArray<FSkinnedVertex>& Vertices = MeshData->Vertices;

    // Initialize influence map for all bones
    OutInfluenceMap.Empty();
    OutInfluenceMap.SetNum(Skeleton.Bones.Num());

    // UE_LOG("BuildBoneVertexInfluenceMap: Processing %d vertices for %d bones", Vertices.Num(), Skeleton.Bones.Num());

    // Iterate through all vertices and build reverse mapping
    for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
    {
        const FSkinnedVertex& Vertex = Vertices[VertexIndex];

        // Check all 4 possible bone influences
        for (int32 i = 0; i < 4; ++i)
        {
            uint32 BoneIndex = Vertex.BoneIndices[i];
            float Weight = Vertex.BoneWeights[i];

            // Only consider vertices with significant weight
            if (Weight >= MinWeightThreshold && BoneIndex < (uint32)Skeleton.Bones.Num())
            {
                OutInfluenceMap[BoneIndex].Vertices.Add(Vertex.Position);
                OutInfluenceMap[BoneIndex].TotalWeight += Weight;
            }
        }
    }

    // Log statistics
    for (int32 BoneIndex = 0; BoneIndex < OutInfluenceMap.Num(); ++BoneIndex)
    {
        const FBoneVertexInfluence& Influence = OutInfluenceMap[BoneIndex];
        if (Influence.Vertices.Num() > 0)
        {
            UE_LOG("  Bone %d (%s): %d influenced vertices (total weight: %.2f)",
                BoneIndex, Skeleton.Bones[BoneIndex].Name.c_str(), Influence.Vertices.Num(), Influence.TotalWeight);
        }
    }
}

FVector SPhysicsAssetEditorWindow::CalculatePrincipalAxis(const TArray<FVector>& Vertices) const
{
    if (Vertices.Num() < 2)
    {
        return FVector(0.0f, 0.0f, 1.0f);  // Default to Z-axis
    }

    // Calculate centroid
    FVector Centroid = FVector::Zero();
    for (const FVector& V : Vertices)
    {
        Centroid += V;
    }
    Centroid /= static_cast<float>(Vertices.Num());

    // Build covariance matrix
    float Cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

    for (const FVector& V : Vertices)
    {
        FVector Diff = V - Centroid;
        Cov[0][0] += Diff.X * Diff.X;
        Cov[0][1] += Diff.X * Diff.Y;
        Cov[0][2] += Diff.X * Diff.Z;
        Cov[1][1] += Diff.Y * Diff.Y;
        Cov[1][2] += Diff.Y * Diff.Z;
        Cov[2][2] += Diff.Z * Diff.Z;
    }

    // Covariance matrix is symmetric
    Cov[1][0] = Cov[0][1];
    Cov[2][0] = Cov[0][2];
    Cov[2][1] = Cov[1][2];

    // Simple power iteration to find principal eigenvector (largest eigenvalue)
    FVector Eigenvector(1.0f, 0.0f, 0.0f);  // Initial guess

    for (int32 Iter = 0; Iter < 20; ++Iter)  // 20 iterations usually enough for convergence
    {
        // Multiply covariance matrix by eigenvector
        FVector NewVec(
            Cov[0][0] * Eigenvector.X + Cov[0][1] * Eigenvector.Y + Cov[0][2] * Eigenvector.Z,
            Cov[1][0] * Eigenvector.X + Cov[1][1] * Eigenvector.Y + Cov[1][2] * Eigenvector.Z,
            Cov[2][0] * Eigenvector.X + Cov[2][1] * Eigenvector.Y + Cov[2][2] * Eigenvector.Z
        );

        // Normalize
        float Length = NewVec.Size();
        if (Length > 0.0001f)
        {
            Eigenvector = NewVec / Length;
        }
        else
        {
            break;
        }
    }

    return Eigenvector;
}

void SPhysicsAssetEditorWindow::FitMinimalSphere(const TArray<FVector>& Vertices, FVector& OutCenter, float& OutRadius) const
{
    if (Vertices.Num() == 0)
    {
        OutCenter = FVector::Zero();
        OutRadius = 5.0f;
        return;
    }

    // Calculate centroid as sphere center
    OutCenter = FVector::Zero();
    for (const FVector& V : Vertices)
    {
        OutCenter += V;
    }
    OutCenter /= static_cast<float>(Vertices.Num());

    // Find maximum distance from centroid
    OutRadius = 0.0f;
    for (const FVector& V : Vertices)
    {
        float Distance = (V - OutCenter).Size();
        if (Distance > OutRadius)
        {
            OutRadius = Distance;
        }
    }

    // Add small padding
    OutRadius *= 1.f;
}

void SPhysicsAssetEditorWindow::FitMinimalCapsule(const TArray<FVector>& Vertices, const FVector& PrincipalAxis,
    FVector& OutCenter, FQuat& OutRotation, float& OutRadius, float& OutHalfHeight) const
{
    if (Vertices.Num() == 0)
    {
        OutCenter = FVector::Zero();
        OutRotation = FQuat::Identity();
        OutRadius = 5.0f;
        OutHalfHeight = 10.0f;
        return;
    }

    // Calculate centroid
    OutCenter = FVector::Zero();
    for (const FVector& V : Vertices)
    {
        OutCenter += V;
    }
    OutCenter /= static_cast<float>(Vertices.Num());

    // Project vertices onto principal axis
    float MinProjection = FLT_MAX;
    float MaxProjection = -FLT_MAX;
    float MaxRadialDistance = 0.0f;

    for (const FVector& V : Vertices)
    {
        FVector Diff = V - OutCenter;
        float AxialProjection = FVector::Dot(Diff, PrincipalAxis);

        MinProjection = FMath::Min(MinProjection, AxialProjection);
        MaxProjection = FMath::Max(MaxProjection, AxialProjection);

        // Calculate radial distance (perpendicular to axis)
        FVector AxialComponent = PrincipalAxis * AxialProjection;
        FVector RadialComponent = Diff - AxialComponent;
        float RadialDistance = RadialComponent.Size();
        MaxRadialDistance = FMath::Max(MaxRadialDistance, RadialDistance);
    }

    // Capsule dimensions
    OutRadius = MaxRadialDistance * 1.0f;  // Add 5% padding
    float CylinderHeight = (MaxProjection - MinProjection) * 1.0f;
    OutHalfHeight = CylinderHeight * 0.5f;

    // Calculate rotation from Z-axis to principal axis
    FVector DefaultDirection(0.0f, 0.0f, 1.0f);
    FVector Axis = FVector::Cross(DefaultDirection, PrincipalAxis);
    float AxisLength = Axis.Size();

    if (AxisLength > 0.0001f)
    {
        Axis /= AxisLength;
        float DotProduct = FVector::Dot(DefaultDirection, PrincipalAxis);
        float Angle = acosf(FMath::Clamp(DotProduct, -1.0f, 1.0f));

        float HalfAngle = Angle * 0.5f;
        float SinHalfAngle = sinf(HalfAngle);
        float CosHalfAngle = cosf(HalfAngle);
        OutRotation = FQuat(Axis.X * SinHalfAngle, Axis.Y * SinHalfAngle, Axis.Z * SinHalfAngle, CosHalfAngle);
    }
    else
    {
        OutRotation = FQuat::Identity();
    }
}

void SPhysicsAssetEditorWindow::FitMinimalBox(const TArray<FVector>& Vertices, const FVector& PrincipalAxis,
    FVector& OutCenter, FQuat& OutRotation, FVector& OutExtent) const
{
    if (Vertices.Num() == 0)
    {
        OutCenter = FVector::Zero();
        OutRotation = FQuat::Identity();
        OutExtent = FVector(5.0f, 5.0f, 10.0f);
        return;
    }

    // Calculate centroid
    OutCenter = FVector::Zero();
    for (const FVector& V : Vertices)
    {
        OutCenter += V;
    }
    OutCenter /= static_cast<float>(Vertices.Num());

    // Build orthonormal basis with principal axis as Z
    FVector ZAxis = PrincipalAxis;
    ZAxis.Normalize();

    // Choose arbitrary perpendicular vector
    FVector XAxis = FVector(1.0f, 0.0f, 0.0f);
    if (FMath::Abs(FVector::Dot(XAxis, ZAxis)) > 0.9f)
    {
        XAxis = FVector(0.0f, 1.0f, 0.0f);
    }

    // Gram-Schmidt orthogonalization
    XAxis = XAxis - ZAxis * FVector::Dot(XAxis, ZAxis);
    XAxis.Normalize();

    FVector YAxis = FVector::Cross(ZAxis, XAxis);
    YAxis.Normalize();

    // Project vertices onto local axes and find extents
    float MinX = FLT_MAX, MaxX = -FLT_MAX;
    float MinY = FLT_MAX, MaxY = -FLT_MAX;
    float MinZ = FLT_MAX, MaxZ = -FLT_MAX;

    for (const FVector& V : Vertices)
    {
        FVector Diff = V - OutCenter;
        float ProjX = FVector::Dot(Diff, XAxis);
        float ProjY = FVector::Dot(Diff, YAxis);
        float ProjZ = FVector::Dot(Diff, ZAxis);

        MinX = FMath::Min(MinX, ProjX);
        MaxX = FMath::Max(MaxX, ProjX);
        MinY = FMath::Min(MinY, ProjY);
        MaxY = FMath::Max(MaxY, ProjY);
        MinZ = FMath::Min(MinZ, ProjZ);
        MaxZ = FMath::Max(MaxZ, ProjZ);
    }

    // Calculate half extents with padding
    OutExtent = FVector(
        (MaxX - MinX) * 0.5f * 1.f,
        (MaxY - MinY) * 0.5f * 1.f,
        (MaxZ - MinZ) * 0.5f * 1.f
    );

    // Calculate rotation from default axes to local axes
    FVector DefaultZ(0.0f, 0.0f, 1.0f);
    FVector RotAxis = FVector::Cross(DefaultZ, ZAxis);
    float RotAxisLength = RotAxis.Size();

    if (RotAxisLength > 0.0001f)
    {
        RotAxis /= RotAxisLength;
        float DotProduct = FVector::Dot(DefaultZ, ZAxis);
        float Angle = acosf(FMath::Clamp(DotProduct, -1.0f, 1.0f));

        float HalfAngle = Angle * 0.5f;
        float SinHalfAngle = sinf(HalfAngle);
        float CosHalfAngle = cosf(HalfAngle);
        OutRotation = FQuat(RotAxis.X * SinHalfAngle, RotAxis.Y * SinHalfAngle, RotAxis.Z * SinHalfAngle, CosHalfAngle);
    }
    else
    {
        OutRotation = FQuat::Identity();
    }
}

void SPhysicsAssetEditorWindow::StartSimulation()
{
    if (!ActiveState || !ActiveState->PreviewActor)
        return;

    UWorld* PreviewWorld = ActiveState->World;
    if (!PreviewWorld)
        return;

    // 1. PreviewWorld의 Physics 활성화
    PreviewWorld->EnablePhysicsSimulation(true);

    // 2. 바닥의 물리 상태 생성 (정적 물리 바디)
    // 바닥 액터는 ViewerState 생성 시 이미 스폰됨
    if (ActiveState->FloorActor)
    {
        if (UBoxComponent* BoxComp = ActiveState->FloorActor->GetBoxComponent())
        {
            BoxComp->CreatePhysicsState();
        }
    }

    // 3. SkeletalMeshComponent의 물리 상태 생성 및 시뮬레이션 활성화
    if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        // PrimitiveComponent::BeginPlay() 패턴에 맞게 프로퍼티를 직접 설정
        // (프로퍼티가 단일 진실 원천, CreatePhysicsState에서 이 값을 참조)
        MeshComp->bSimulatePhysics = true;
        MeshComp->bEnableGravity = true;
		MeshComp->bEnableRagdoll = true;

        // 물리 상태 생성 (Bodies + Constraints) - bSimulatePhysics=true 상태로 Dynamic Actor 생성
        MeshComp->CreatePhysicsState();

        // 래그돌 모드 활성화 - bSimulatingRagdoll을 true로 설정해야
        // TickComponent에서 UpdateBoneTransformsFromPhysics()가 호출됨
        // MeshComp->SetAllBodiesSimulatePhysics(true);
    }

    bIsSimulating = true;
}

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
        MeshComp->ResetToRefPose();

        // 4. 캐릭터를 원점(바닥 위)으로 복원
        ActiveState->PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
    }

    // 5. 바닥의 물리 상태 정리 (바닥 액터는 유지)
    if (ActiveState->FloorActor)
    {
        if (UBoxComponent* BoxComp = ActiveState->FloorActor->GetBoxComponent())
        {
            BoxComp->DestroyPhysicsState();
        }
    }

    // 6. PhysScene 비활성화
    // 시뮬레이션 중지 시에는 PhysScene도 정리해야 함.
    // DestroyPhysicsState() 후 PhysScene의 active actors 캐시에 dangling 포인터가 남아있어
    // 다음 UWorld::Tick()의 EndFrame()에서 크래시가 발생할 수 있음.
    //
    // 주의: DestroyViewerState()에서는 EnablePhysicsSimulation(false)를 호출하면 안 됨!
    //       (World 삭제 전에 PhysScene이 먼저 삭제되면 Actor 삭제 시 크래시)
    //       하지만 StopSimulation()에서는 Editor가 계속 실행되므로 안전함.
    if (ActiveState->World)
    {
        ActiveState->World->EnablePhysicsSimulation(false);
    }

    bIsSimulating = false;
}