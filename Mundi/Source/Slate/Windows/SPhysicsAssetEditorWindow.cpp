#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "SlateManager.h"
#include "FViewport.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"

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
            float propertiesHeight = ImGui::GetContentRegionAvail().y * 0.5f;

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
        // 누적 방지를 위해 먼저 참조 포즈로 리셋
        MeshComp->ResetToRefPose();

        if (!ActiveState->BoneAdditiveTransforms.IsEmpty())
        {
            MeshComp->ApplyAdditiveTransforms(ActiveState->BoneAdditiveTransforms);
        }
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

void SPhysicsAssetEditorWindow::RenderHierarchySection()
{
    RenderPhysicsBodyHierarchy();
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
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::IsItemClicked())
            {
                ActiveState->SelectedBoneIndex = BoneIndex;
                ActiveState->SelectedBodyIndex = -1; // Deselect any body
                bCollisionShapesDirty = true;
            }
            if (bHasBody) { /* Options for bone when it has a body */ }
            else
            {
                if (ImGui::MenuItem("Create Body")) { CreateBodyForBone(BoneIndex, SelectedPrimitiveType); }
                if (ImGui::MenuItem("Create Constraint")) { /* TODO */ }
            }
            ImGui::EndPopup();
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            ActiveState->SelectedBoneIndex = BoneIndex;
            ActiveState->SelectedBodyIndex = -1;
            bCollisionShapesDirty = true;
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
                    int primitiveCount = BodySetup->AggGeom.SphereElems.Num() + BodySetup->AggGeom.BoxElems.Num() + BodySetup->AggGeom.SphylElems.Num();
                    if (primitiveCount == 1)
                    {
                        if (BodySetup->AggGeom.SphereElems.Num() == 1) BodyLabel = "Sphere";
                        else if (BodySetup->AggGeom.BoxElems.Num() == 1) BodyLabel = "Box";
                        else if (BodySetup->AggGeom.SphylElems.Num() == 1) BodyLabel = "Capsule";
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
                        // TODO: Select primitive as well?
                        bCollisionShapesDirty = true;
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                         if (ImGui::IsItemClicked()) {
                            ActiveState->SelectedBodyIndex = BodyIndex;
                            ActiveState->SelectedBoneIndex = BoneIndex;
                            bCollisionShapesDirty = true;
                        }
                        if (ImGui::MenuItem("Add Primitive...")) { /* TODO */ }
                        ImGui::EndPopup();
                    }
                }
            }

            // 2. Render child bones recursively
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
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 5));

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

            // Clear visualization
            bCollisionShapesDirty = true;
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

        // Create or load PhysicsAsset for this skeletal mesh
        if (!State->CurrentPhysicsAsset)
        {
            State->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();
            UE_LOG("SPhysicsAssetEditorWindow: Created new PhysicsAsset for %s", Path.c_str());
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

void SPhysicsAssetEditorWindow::DrawWireframeBox(ULineComponent* LineComp, const FVector& Center, const FVector& HalfExtents, const FVector4& Color)
{
    if (!LineComp) return;

    // 8 corners of the box
    FVector corners[8] = {
        Center + FVector( HalfExtents.X,  HalfExtents.Y,  HalfExtents.Z),  // 0: +X +Y +Z
        Center + FVector( HalfExtents.X,  HalfExtents.Y, -HalfExtents.Z),  // 1: +X +Y -Z
        Center + FVector( HalfExtents.X, -HalfExtents.Y,  HalfExtents.Z),  // 2: +X -Y +Z
        Center + FVector( HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z),  // 3: +X -Y -Z
        Center + FVector(-HalfExtents.X,  HalfExtents.Y,  HalfExtents.Z),  // 4: -X +Y +Z
        Center + FVector(-HalfExtents.X,  HalfExtents.Y, -HalfExtents.Z),  // 5: -X +Y -Z
        Center + FVector(-HalfExtents.X, -HalfExtents.Y,  HalfExtents.Z),  // 6: -X -Y +Z
        Center + FVector(-HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z)   // 7: -X -Y -Z
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

void SPhysicsAssetEditorWindow::DrawWireframeSphere(ULineComponent* LineComp, const FVector& Center, float Radius, const FVector4& Color, int32 Segments)
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
        FVector p0_xy = Center + FVector(Radius * cos0, Radius * sin0, 0.0f);
        FVector p1_xy = Center + FVector(Radius * cos1, Radius * sin1, 0.0f);
        LineComp->AddLine(p0_xy, p1_xy, Color);

        // XZ plane (Y = 0)
        FVector p0_xz = Center + FVector(Radius * cos0, 0.0f, Radius * sin0);
        FVector p1_xz = Center + FVector(Radius * cos1, 0.0f, Radius * sin1);
        LineComp->AddLine(p0_xz, p1_xz, Color);

        // YZ plane (X = 0)
        FVector p0_yz = Center + FVector(0.0f, Radius * cos0, Radius * sin0);
        FVector p1_yz = Center + FVector(0.0f, Radius * cos1, Radius * sin1);
        LineComp->AddLine(p0_yz, p1_yz, Color);
    }
}

void SPhysicsAssetEditorWindow::DrawWireframeCapsule(ULineComponent* LineComp, const FVector& Center, float Radius, float HalfHeight, const FVector4& Color, int32 Segments)
{
    if (!LineComp || Segments < 3) return;

    const float TWO_PI = 6.28318530718f;

    // Capsule is oriented along Z-axis
    // Top hemisphere center
    FVector TopCenter = Center + FVector(0.0f, 0.0f, HalfHeight);
    // Bottom hemisphere center
    FVector BottomCenter = Center - FVector(0.0f, 0.0f, HalfHeight);

    // Draw cylinder body (4 vertical lines connecting top and bottom circles)
    int32 numVerticalLines = 4;
    for (int32 i = 0; i < numVerticalLines; ++i)
    {
        float angle = (static_cast<float>(i) / static_cast<float>(numVerticalLines)) * TWO_PI;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        FVector topPoint = TopCenter + FVector(Radius * cosA, Radius * sinA, 0.0f);
        FVector bottomPoint = BottomCenter + FVector(Radius * cosA, Radius * sinA, 0.0f);
        LineComp->AddLine(topPoint, bottomPoint, Color);
    }

    // Draw top and bottom circles (XY plane)
    for (int32 i = 0; i < Segments; ++i)
    {
        float angle0 = (static_cast<float>(i) / static_cast<float>(Segments)) * TWO_PI;
        float angle1 = (static_cast<float>((i + 1) % Segments) / static_cast<float>(Segments)) * TWO_PI;

        float cos0 = cosf(angle0);
        float sin0 = sinf(angle0);
        float cos1 = cosf(angle1);
        float sin1 = sinf(angle1);

        // Top circle
        FVector p0_top = TopCenter + FVector(Radius * cos0, Radius * sin0, 0.0f);
        FVector p1_top = TopCenter + FVector(Radius * cos1, Radius * sin1, 0.0f);
        LineComp->AddLine(p0_top, p1_top, Color);

        // Bottom circle
        FVector p0_bottom = BottomCenter + FVector(Radius * cos0, Radius * sin0, 0.0f);
        FVector p1_bottom = BottomCenter + FVector(Radius * cos1, Radius * sin1, 0.0f);
        LineComp->AddLine(p0_bottom, p1_bottom, Color);
    }

    // Draw hemisphere arcs (XZ and YZ planes)
    int32 arcSegments = Segments / 2;
    for (int32 i = 0; i <= arcSegments; ++i)
    {
        float angle0 = (static_cast<float>(i) / static_cast<float>(arcSegments)) * PI;
        float angle1 = (static_cast<float>(i + 1) / static_cast<float>(arcSegments)) * PI;

        float cos0 = cosf(angle0);
        float sin0 = sinf(angle0);
        float cos1 = cosf(angle1);
        float sin1 = sinf(angle1);

        // Top hemisphere - XZ plane arc
        if (i < arcSegments)
        {
            FVector p0_top_xz = TopCenter + FVector(Radius * cos0, 0.0f, Radius * sin0);
            FVector p1_top_xz = TopCenter + FVector(Radius * cos1, 0.0f, Radius * sin1);
            LineComp->AddLine(p0_top_xz, p1_top_xz, Color);

            // Top hemisphere - YZ plane arc
            FVector p0_top_yz = TopCenter + FVector(0.0f, Radius * cos0, Radius * sin0);
            FVector p1_top_yz = TopCenter + FVector(0.0f, Radius * cos1, Radius * sin1);
            LineComp->AddLine(p0_top_yz, p1_top_yz, Color);

            // Bottom hemisphere - XZ plane arc
            FVector p0_bottom_xz = BottomCenter + FVector(Radius * cos0, 0.0f, -Radius * sin0);
            FVector p1_bottom_xz = BottomCenter + FVector(Radius * cos1, 0.0f, -Radius * sin1);
            LineComp->AddLine(p0_bottom_xz, p1_bottom_xz, Color);

            // Bottom hemisphere - YZ plane arc
            FVector p0_bottom_yz = BottomCenter + FVector(0.0f, Radius * cos0, -Radius * sin0);
            FVector p1_bottom_yz = BottomCenter + FVector(0.0f, Radius * cos1, -Radius * sin1);
            LineComp->AddLine(p0_bottom_yz, p1_bottom_yz, Color);
        }
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
        FVector BoneWorldPos = BoneTransform.Translation;
        FQuat BoneWorldRot = BoneTransform.Rotation;

        // Choose color based on selection
        FVector4 DrawColor = (BodyIndex == ActiveState->SelectedBodyIndex) ? SelectedColor : CollisionColor;

        // Draw spheres
        for (const FSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
        {
            // Transform center to world space
            FVector WorldCenter = BoneTransform.TransformPosition(SphereElem.Center);
            DrawWireframeSphere(LineComp, WorldCenter, SphereElem.Radius, DrawColor, 16);
        }

        // Draw boxes
        for (const FBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
        {
            // Transform center and rotation to world space
            FVector WorldCenter = BoneTransform.TransformPosition(BoxElem.Center);
            FQuat WorldRotation = BoneWorldRot * BoxElem.Rotation;

            // Box half extents (BoxElem stores full dimensions)
            FVector HalfExtents(BoxElem.X * 0.5f, BoxElem.Y * 0.5f, BoxElem.Z * 0.5f);

            // Draw box with rotation (need to implement rotated box drawing)
            // For now, draw axis-aligned box at transformed center
            DrawWireframeBox(LineComp, WorldCenter, HalfExtents, DrawColor);
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
            DrawWireframeCapsule(LineComp, WorldCenter, CapsuleElem.Radius, HalfHeight, DrawColor, 16);
        }
    }

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

void SPhysicsAssetEditorWindow::GenerateAllBodies(EPrimitiveType PrimitiveType)
{
    if (!ActiveState || !ActiveState->CurrentMesh || !ActiveState->CurrentPhysicsAsset)
    {
        UE_LOG("GenerateAllBodies: Invalid state - missing mesh or physics asset");
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.IsEmpty())
    {
        UE_LOG("GenerateAllBodies: Skeleton is empty or invalid");
        return;
    }

    // Get skeletal mesh data for vertex information
    const FSkeletalMeshData* MeshData = ActiveState->CurrentMesh->GetSkeletalMeshData();
    if (!MeshData || MeshData->Vertices.Num() == 0)
    {
        UE_LOG("GenerateAllBodies: No vertex data available - cannot use vertex-driven generation");
        return;
    }

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;

    // Clear all existing bodies before regenerating
    UE_LOG("GenerateAllBodies: Clearing existing bodies");
    PhysicsAsset->ClearAllBodies();

    UE_LOG("GenerateAllBodies: Starting vertex-driven body generation (UE style) for %d bones", Skeleton->Bones.Num());

    // Step 1: Build bone-to-vertex influence map
    TArray<FBoneVertexInfluence> InfluenceMap;
    BuildBoneVertexInfluenceMap(MeshData, InfluenceMap, 0.3f);  // 30% weight threshold

    // Step 2: Create bodies for bones with sufficient vertex influence
    const TArray<FBone>& Bones = Skeleton->Bones;
    int32 CreatedCount = 0;

    for (int32 BoneIndex = 0; BoneIndex < Bones.size(); ++BoneIndex)
    {
        const FBone& Bone = Bones[BoneIndex];
        FName BoneName(Bone.Name);

        // Filter: Skip bones that shouldn't have physics bodies
        if (!ShouldCreateBodyForBone(BoneIndex, Skeleton))
        {
            continue;
        }

        // Check if this bone has enough influenced vertices
        const FBoneVertexInfluence& Influence = InfluenceMap[BoneIndex];
        if (Influence.Vertices.Num() < 3)  // Need at least 3 vertices for meaningful shape
        {
            UE_LOG("GenerateAllBodies: Skipping bone %s - only %d influenced vertices",
                Bone.Name.c_str(), Influence.Vertices.Num());
            continue;
        }

        UE_LOG("GenerateAllBodies: Processing bone %s with %d influenced vertices",
            Bone.Name.c_str(), Influence.Vertices.Num());

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
        FVector PrincipalAxis = CalculatePrincipalAxis(Influence.Vertices);

        // Step 4: Fit minimal bounding primitive based on type
        FVector Center;
        FQuat Rotation;
        float Radius, HalfHeight;
        FVector Extent;

        switch (PrimitiveType)
        {
        case EPrimitiveType::Sphere:
            FitMinimalSphere(Influence.Vertices, Center, Radius);
            NewBody->SphereRadius = Radius;
            {
                FSphereElem SphereElem(Radius);
                SphereElem.Center = Center;
                NewBody->AggGeom.SphereElems.Add(SphereElem);
            }
            UE_LOG("  -> Sphere: Center=(%.2f,%.2f,%.2f), Radius=%.2f",
                Center.X, Center.Y, Center.Z, Radius);
            break;

        case EPrimitiveType::Box:
            FitMinimalBox(Influence.Vertices, PrincipalAxis, Center, Rotation, Extent);
            NewBody->BoxExtent = Extent;
            {
                FBoxElem BoxElem(Extent.X * 2.0f, Extent.Y * 2.0f, Extent.Z * 2.0f);
                BoxElem.Center = Center;
                BoxElem.Rotation = Rotation;
                NewBody->AggGeom.BoxElems.Add(BoxElem);
            }
            UE_LOG("  -> Box: Center=(%.2f,%.2f,%.2f), Extent=(%.2f,%.2f,%.2f)",
                Center.X, Center.Y, Center.Z, Extent.X, Extent.Y, Extent.Z);
            break;

        case EPrimitiveType::Capsule:
            FitMinimalCapsule(Influence.Vertices, PrincipalAxis, Center, Rotation, Radius, HalfHeight);
            NewBody->CapsuleHalfHeight = HalfHeight;
            NewBody->SphereRadius = Radius;
            {
                float CapsuleLength = HalfHeight * 2.0f;
                FSphylElem CapsuleElem(Radius, CapsuleLength);
                CapsuleElem.Center = Center;
                CapsuleElem.Rotation = Rotation;
                NewBody->AggGeom.SphylElems.Add(CapsuleElem);
            }
            UE_LOG("  -> Capsule: Center=(%.2f,%.2f,%.2f), Radius=%.2f, HalfHeight=%.2f",
                Center.X, Center.Y, Center.Z, Radius, HalfHeight);
            break;

        default:
            UE_LOG("  -> Unknown primitive type");
            delete NewBody;
            continue;
        }

        // Add the body to the physics asset
        PhysicsAsset->AddBodySetup(NewBody);
        CreatedCount++;
    }

    // Update the body index map
    PhysicsAsset->UpdateBodySetupIndexMap();

    UE_LOG("GenerateAllBodies: Completed vertex-driven generation - created %d bodies", CreatedCount);

    // Mark collision shapes dirty to visualize the newly created bodies
    bCollisionShapesDirty = true;
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

        // Exclude bones shorter than 0.01 units (1cm - excludes finger segments)
        // Mixamo models are often scaled down, so use a small threshold
        if (BoneLength < 0.05f)
        {
            UE_LOG("ShouldCreateBodyForBone: Excluding short bone '%s' (length: %.2f)", Bone.Name.c_str(), BoneLength);
            return false;
        }
    }

    // Passed all filters - create body for this bone
    UE_LOG("ShouldCreateBodyForBone: Creating body for '%s'", Bone.Name.c_str());
    return true;
}

void SPhysicsAssetEditorWindow::CreateBodyForBone(int32 BoneIndex, EPrimitiveType PrimitiveType)
{
    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        UE_LOG("CreateBodyForBone: Invalid state - missing mesh.");
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
    {
        UE_LOG("CreateBodyForBone: Invalid bone index or skeleton.");
        return;
    }

    // If no PhysicsAsset exists yet, create one.
    if (!ActiveState->CurrentPhysicsAsset)
    {
        ActiveState->CurrentPhysicsAsset = NewObject<UPhysicsAsset>();
        UE_LOG("CreateBodyForBone: Created new PhysicsAsset for skeletal mesh.");
    }

    UPhysicsAsset* PhysicsAsset = ActiveState->CurrentPhysicsAsset;
    const FBone& Bone = Skeleton->Bones[BoneIndex];
    FName BoneName(Bone.Name);

    // Check if a body already exists for this bone
    if (PhysicsAsset->FindBodyIndex(BoneName) >= 0)
    {
        UE_LOG("CreateBodyForBone: Bone '%s' already has a physics body. Cannot create another one.", Bone.Name.c_str());
        return;
    }
    
    UE_LOG("CreateBodyForBone: Creating body for bone '%s' (BoneIndex: %d) with primitive type %d.", Bone.Name.c_str(), BoneIndex, (int)PrimitiveType);

    // Create new BodySetup
    UBodySetup* NewBody = NewObject<UBodySetup>();
    if (!NewBody)
    {
        UE_LOG("CreateBodyForBone: Failed to create BodySetup for bone '%s'.", Bone.Name.c_str());
        return;
    }

    NewBody->BoneName = BoneName;
    NewBody->BodyType = ToBodySetupType(PrimitiveType); // Assuming ToBodySetupType is accessible

    float OutRadius = 0.0f;
    float OutHalfHeight = 0.0f;
    FVector OutExtent = FVector::Zero();

    // Calculate dimensions based on bone length using the existing helper function
    CalculateBodyDimensions(BoneIndex, Skeleton, PrimitiveType, OutRadius, OutHalfHeight, OutExtent);

    // Add primitive element based on type
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
        {
            FSphereElem SphereElem(OutRadius);
            SphereElem.Center = FVector::Zero(); // Center at bone origin for now
            NewBody->AggGeom.SphereElems.Add(SphereElem);
            UE_LOG("  -> Sphere: Center=(%.2f,%.2f,%.2f), Radius=%.2f",
                SphereElem.Center.X, SphereElem.Center.Y, SphereElem.Center.Z, SphereElem.Radius);
        }
        break;

    case EPrimitiveType::Box:
    {
    FBoxElem BoxElem(OutExtent.X * 2.0f, OutExtent.Y * 2.0f, OutExtent.Z * 2.0f); // BoxElem expects full dimensions
    BoxElem.Center = FVector::Zero(); // Center at bone origin for now
    BoxElem.Rotation = FQuat::Identity(); // No rotation for now
    NewBody->AggGeom.BoxElems.Add(BoxElem);
    UE_LOG("  -> Box: Center=(%.2f,%.2f,%.2f), Extent=(%.2f,%.2f,%.2f)",
        BoxElem.Center.X, BoxElem.Center.Y, BoxElem.Center.Z, OutExtent.X, OutExtent.Y, OutExtent.Z);
    }
    break;
    
    case EPrimitiveType::Capsule:
    {
        float CapsuleLength = OutHalfHeight * 2.0f; // FSphylElem expects full length
        FSphylElem CapsuleElem(OutRadius, CapsuleLength);
        CapsuleElem.Center = FVector::Zero(); // Center at bone origin for now
        CapsuleElem.Rotation = FQuat::Identity(); // No rotation for now
        NewBody->AggGeom.SphylElems.Add(CapsuleElem);
        UE_LOG("  -> Capsule: Center=(%.2f,%.2f,%.2f), Radius=%.2f, HalfHeight=%.2f",
            CapsuleElem.Center.X, CapsuleElem.Center.Y, CapsuleElem.Center.Z, CapsuleElem.Radius, OutHalfHeight);
    }
    break;
    
    default:
        UE_LOG("  -> Unknown primitive type for body creation.");
        return;
    }
    
    // Add the body to the physics asset
    PhysicsAsset->AddBodySetup(NewBody);
    PhysicsAsset->UpdateBodySetupIndexMap(); // Rebuild internal map
    
    // Select the newly created body
    ActiveState->SelectedBoneIndex = BoneIndex;
    ActiveState->SelectedBodyIndex = PhysicsAsset->FindBodyIndex(BoneName); // Re-find the index after adding
    ActiveState->bBoneLinesDirty = true;
    bCollisionShapesDirty = true; // Mark collision shapes dirty to visualize
    
    UE_LOG("CreateBodyForBone: Successfully created body for '%s'.", Bone.Name.c_str());
}

void SPhysicsAssetEditorWindow::CalculateBodyDimensions(int32 BoneIndex, const FSkeleton* Skeleton, EPrimitiveType PrimitiveType,
                                                         float& OutRadius, float& OutHalfHeight, FVector& OutExtent) const
{
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
    {
        // Default fallback dimensions
        OutRadius = 5.0f;
        OutHalfHeight = 10.0f;
        OutExtent = FVector(5.0f, 5.0f, 10.0f);
        return;
    }

    const TArray<FBone>& Bones = Skeleton->Bones;
    const FBone& Bone = Bones[BoneIndex];

    // Find first child bone
    int32 FirstChildIndex = -1;
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        if (Bones[i].ParentIndex == BoneIndex)
        {
            FirstChildIndex = i;
            break;
        }
    }

    float BoneLength = 10.0f;  // Default length for leaf bones

    // Calculate bone length if there's a child
    if (FirstChildIndex >= 0)
    {
        // Get bone positions from bind pose (translation is in M[3][0..2])
        FVector BonePos = FVector(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);
        FVector ChildPos = FVector(
            Bones[FirstChildIndex].BindPose.M[3][0],
            Bones[FirstChildIndex].BindPose.M[3][1],
            Bones[FirstChildIndex].BindPose.M[3][2]
        );

        // Calculate distance between bone and child
        BoneLength = (ChildPos - BonePos).Size();

        // Clamp to reasonable values
        BoneLength = FMath::Clamp(BoneLength, 1.0f, 100.0f);
    }

    // Calculate dimensions based on primitive type
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
        // Sphere radius is a fraction of bone length
        OutRadius = BoneLength * 0.3f;
        OutHalfHeight = 0.0f;
        OutExtent = FVector(OutRadius, OutRadius, OutRadius);
        break;

    case EPrimitiveType::Box:
        // Box dimensions based on bone length
        // Make it elongated along the bone direction (Z-axis)
        OutRadius = 0.0f;
        OutHalfHeight = 0.0f;
        OutExtent = FVector(
            BoneLength * 0.15f,  // X half extent
            BoneLength * 0.15f,  // Y half extent
            BoneLength * 0.5f    // Z half extent (along bone)
        );
        break;

    case EPrimitiveType::Capsule:
        // Capsule oriented along bone
        // Radius is proportional to bone thickness
        OutRadius = BoneLength * 0.15f;
        // Half height is half the cylinder portion (excluding the hemisphere caps)
        OutHalfHeight = BoneLength * 0.35f;
        OutExtent = FVector(OutRadius, OutRadius, OutHalfHeight);
        break;
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

    UE_LOG("BuildBoneVertexInfluenceMap: Processing %d vertices for %d bones", Vertices.Num(), Skeleton.Bones.Num());

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
    OutRadius *= 1.05f;
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
    OutRadius = MaxRadialDistance * 1.05f;  // Add 5% padding
    float CylinderHeight = (MaxProjection - MinProjection) * 1.05f;
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
        (MaxX - MinX) * 0.5f * 1.05f,
        (MaxY - MinY) * 0.5f * 1.05f,
        (MaxZ - MinZ) * 0.5f * 1.05f
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