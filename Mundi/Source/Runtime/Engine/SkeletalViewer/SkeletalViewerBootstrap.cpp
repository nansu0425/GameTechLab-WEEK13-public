#include "pch.h"
#include "SkeletalViewerBootstrap.h"
#include "CameraActor.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "FViewport.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Animation/AnimTestUtil.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"

ViewerState* SkeletalViewerBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    ViewerState* State = new ViewerState();
    State->Name = Name ? Name : "Viewer";

    // Preview world 만들기
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);  // Set as preview world for memory optimization
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    State->World->GetGizmoActor()->SetSpace(EGizmoSpace::Local);
    
    // Viewport + client per tab
    State->Viewport = new FViewport();
    // 프레임 마다 initial size가 바꿜 것이다
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor (mesh can be set later from UI)
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;

        // Placeholder: create a simple test sequence and queue playback.
        // Note: It will start visibly playing once a skeletal mesh is assigned in the viewer UI.
        if (Preview && Preview->GetSkeletalMeshComponent())
        {
            // If there is already a mesh with skeleton, build sequence to match; otherwise build generic.
            const USkeletalMesh* Mesh = Preview->GetSkeletalMeshComponent()->GetSkeletalMesh();
            const FSkeleton* Skel = Mesh ? Mesh->GetSkeleton() : nullptr;
            if (Skel)
            {
                if (UAnimSequence* TestSeq = AnimTestUtil::CreateSimpleSwingSequence(*Skel, 1.0f, 30.0f))
                {
                    Preview->GetSkeletalMeshComponent()->PlayAnimation(TestSeq, true, 1.0f);
                }
            }
            else
            {
                // Without a mesh yet, we can still prepare a 1-bone sequence; playback starts after a mesh is set.
                FSkeleton EmptySkel; // no bones yet
                if (UAnimSequence* TestSeq = AnimTestUtil::CreateSimpleSwingSequence(EmptySkel, 1.0f, 30.0f))
                {
                    Preview->GetSkeletalMeshComponent()->PlayAnimation(TestSeq, true, 1.0f);
                }
            }
        }
    }

    return State;
}

void SkeletalViewerBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;
    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}
