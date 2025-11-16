#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"

SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SSkeletalMeshViewerWindow::PreRenderViewportUpdate()
{
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

ViewerState* SSkeletalMeshViewerWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    ViewerState* NewState = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!NewState) return nullptr;

    if (Context && !Context->AssetPath.empty())
    {
        LoadSkeletalMesh(NewState, Context->AssetPath);
    }
    return NewState;
}

void SSkeletalMeshViewerWindow::DestroyViewerState(ViewerState*& State)
{
    SkeletalViewerBootstrap::DestroyViewerState(State);
}

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(ViewerState* State, const FString& Path)
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

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}