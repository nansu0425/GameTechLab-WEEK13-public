#include "pch.h"
#include "SAnimationViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/AnimationViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"

SAnimationViewerWindow::SAnimationViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SAnimationViewerWindow::~SAnimationViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        AnimationViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SAnimationViewerWindow::PreRenderViewportUpdate()
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

void SAnimationViewerWindow::LoadSkeletalMesh(const FString& Path)
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

        UE_LOG("SAnimationViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SAnimationViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

ViewerState* SAnimationViewerWindow::CreateViewerState(const char* Name)
{
    return AnimationViewerBootstrap::CreateViewerState(Name, World, Device);
}

void SAnimationViewerWindow::DestroyViewerState(ViewerState*& State)
{
    AnimationViewerBootstrap::DestroyViewerState(State);
}