#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"

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

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(const FString& Path)
{
    if (!ActiveState || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && ActiveState->PreviewActor)
    {
        // Set the mesh on the preview actor
        ActiveState->PreviewActor->SetSkeletalMesh(Path);
        ActiveState->CurrentMesh = Mesh;

        // Expand all bone nodes by default on mesh load
        ActiveState->ExpandedBoneIndices.clear();
        if (const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton())
        {
            for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
            {
                ActiveState->ExpandedBoneIndices.insert(i);
            }
        }

        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading

        // Update mesh path buffer for display in UI
        strncpy_s(ActiveState->MeshPathBuffer, Path.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(ActiveState->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        ActiveState->bBoneLinesDirty = true;

        // Clear and sync bone line visibility
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(ActiveState->bShowBones);
        }

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

ViewerState* SSkeletalMeshViewerWindow::CreateViewerState(const char* Name)
{
    return SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
}

void SSkeletalMeshViewerWindow::DestroyViewerState(ViewerState*& State)
{
    SkeletalViewerBootstrap::DestroyViewerState(State);
}