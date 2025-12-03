#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "CameraActor.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/PhysGroundActor.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/BoxComponent.h"

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
        // 바닥 액터 생성 (시뮬레이션과 무관하게 항상 표시)
        State->FloorActor = State->World->SpawnActor<APhysGroundActor>();
        if (State->FloorActor)
        {
            // 바닥 위 중앙이 원점이 되도록 배치 (바닥 두께 0.1, 절반인 0.05 아래로)
            // 주의: SpawnActor에서 SetActorTransform(FTransform())이 호출되어 스케일이 리셋됨
            //       따라서 위치와 스케일을 명시적으로 다시 설정
            State->FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
            State->FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.1f));  // 넓고 얇은 바닥
        }

        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;

        // 캐릭터를 원점에 배치 (바닥 위)
        if (Preview)
        {
            Preview->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
        }

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

    // ═══════════════════════════════════════════════════════════════════════════
    // 기존 SkeletalViewerBootstrap과 동일한 방식으로 정리
    // World 삭제 시 Level 내 모든 Actor/Component가 자동으로 정리됨
    // PhysScene도 World의 unique_ptr 멤버이므로 World 소멸 시 자동 해제됨
    //
    // 주의: EnablePhysicsSimulation(false)를 명시적으로 호출하면 PhysScene이
    //       먼저 삭제되어 Actor 삭제 시 BodyInstance::TermBody()에서 크래시 발생
    // ═══════════════════════════════════════════════════════════════════════════

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

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath, const FString& LoadedMeshPath)
{
    if (FilePath.empty())
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: FilePath가 비어있습니다.");
        return nullptr;
    }

    FWideString WidePath = UTF8ToWide(FilePath);

    JSON JsonHandle;
    if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 파일 로드 실패: %s", FilePath.c_str());
        return nullptr;
    }
    
    if (!UPhysicsAsset::IsCompatibleWithMesh(LoadedMeshPath, JsonHandle))
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 애셋과 메시가 일치하지 않음: %s", FilePath.c_str());
        return nullptr;
    }
    
    UPhysicsAsset* CachedAsset = UResourceManager::GetInstance().GetPhysicsAsset(FilePath);    
    if (CachedAsset)
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 캐시된 애셋 반환: %s", FilePath.c_str());
        return CachedAsset;
    }

    UPhysicsAsset* NewAsset = NewObject<UPhysicsAsset>();
    if (!NewAsset)
    {
        UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: PhysicsAsset 객체 생성 실패");
        return nullptr;
    }
    NewAsset->Serialize(true, JsonHandle);

    UResourceManager::GetInstance().AddOrReplacePhysicsAsset(FilePath, NewAsset);
    UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 로드 성공: %s", FilePath.c_str());

    return NewAsset;
}
