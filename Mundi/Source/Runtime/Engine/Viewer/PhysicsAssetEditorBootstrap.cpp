#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "CameraActor.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"

ViewerState* PhysicsAssetEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
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
    // ImGui::Image 방식으로 렌더링 (뷰어용)
    State->Viewport->SetUseRenderTarget(true);

    auto* Client = new FViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);

    // Set initial camera position to front view for skeletal mesh viewer
    // Looking at origin (0,0,0) from front, slightly elevated
    ACameraActor* Camera = Client->GetCamera();
    Camera->SetActorLocation(FVector(3, 0, 2));  // Front view, slightly elevated
    Camera->SetActorRotation(FVector(0.0f, 10.0f, 180.0f)); // Front view, slightly looking down

    // Initialize camera rotation state to match the initial rotation
    // This prevents the camera from snapping when first clicked
    Camera->SetCameraPitch(10.0f);   // Pitch: 0 degrees (level)
    Camera->SetCameraYaw(180.0f);   // Yaw: 180 degrees (facing +X direction from -X position)

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor (mesh can be set later from UI)
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;

        // Create a separate LineComponent for collision shape visualization
        State->CollisionShapeLineComponent = NewObject<ULineComponent>();
        State->CollisionShapeLineComponent->ObjectName = "CollisionShapes";
        State->CollisionShapeLineComponent->SetAlwaysOnTop(true);
        Preview->AddOwnedComponent(State->CollisionShapeLineComponent);  // IMPORTANT: Add to actor first!
        State->CollisionShapeLineComponent->RegisterComponent(State->World);
    }

    return State;
}

void PhysicsAssetEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;

    // Clean up collision shape line component before destroying world
    if (State->CollisionShapeLineComponent)
    {
        ObjectFactory::DeleteObject(State->CollisionShapeLineComponent);
        State->CollisionShapeLineComponent = nullptr;
    }

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& Path)
{
    if (!Asset)
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: Asset이 nullptr입니다");
        return false;
    }

    if (Path.empty())
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: FilePath가 비어있습니다");
        return false;
    }

    JSON JsonHandle = JSON::Make(JSON::Class::Object);
    Asset->Serialize(false, JsonHandle);

    FWideString WidePath = UTF8ToWide(Path);
    if (!FJsonSerializer::SaveJsonToFile(JsonHandle, WidePath))
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 파일 저장 실패: %s", Path.c_str());
        return false;
    }

    UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 저장 성공: %s", Path.c_str());
    return true;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& Path)
{
    if (Path.empty())
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: FilePath가 비어있습니다.");
        return nullptr;
    }
    
    UPhysicsAsset* CachedAsset = UResourceManager::GetInstance().GetPhysicsAsset(Path);    
    if (CachedAsset)
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 캐시된 애셋 반환: %s", Path.c_str());
        return CachedAsset;
    }

    FWideString WidePath = UTF8ToWide(Path);
    
    JSON JsonHandle;
    if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 파일 로드 실패: %s", Path.c_str());
        return nullptr;
    }

    UPhysicsAsset* NewAsset = NewObject<UPhysicsAsset>();
    if (!NewAsset)
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: PhysicsAsset 객체 생성 실패");
        return nullptr;
    }
    NewAsset->Serialize(true, JsonHandle);

    UResourceManager::GetInstance().AddOrReplacePhysicsAsset(Path, NewAsset);
    UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 로드 성공: %s", Path.c_str());

    return NewAsset;
}
