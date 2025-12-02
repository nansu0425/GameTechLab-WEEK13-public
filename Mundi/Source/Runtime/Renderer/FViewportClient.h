#pragma once
#include "Vector.h"
#include"Enums.h"

class FViewport;
class UWorld;
class UCameraComponent;
class SViewportWindow;
class AActor;


/**
 * @brief 뷰포트 클라이언트 - UE의 FViewportClient를 모방
 */
class FViewportClient
{
public:
    FViewportClient();
    virtual ~FViewportClient();

    // 렌더링
    virtual void Draw(FViewport* Viewport);
    virtual void Tick(FViewport* Viewport, float DeltaTime);

    // 입력 처리
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y);
    virtual void MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseWheel(FViewport* Viewport, float DeltaSeconds);
    virtual void KeyDown(FViewport* Viewport, int32 KeyCode) {}
    virtual void KeyUp(FViewport* Viewport, int32 KeyCode) {}

    // 뷰포트 설정
    void SetViewportType(EViewportType InType);
    EViewportType GetViewportType() const { return ViewportType; }

    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    void SetCamera(ACameraActor* InCamera) { Camera = InCamera; }
    ACameraActor* GetCamera() const { return Camera; }

    void SetOwnerWindow(SViewportWindow* InOwner) { OwnerWindow = InOwner; }
    SViewportWindow* GetOwnerWindow() const { return OwnerWindow; }

    // 배경색 설정
    void SetBackgroundColor(const FLinearColor& InColor) { BackgroundColor = InColor; }
    const FLinearColor& GetBackgroundColor() const { return BackgroundColor; }

    // 카메라 매트릭스 계산
    FMatrix GetViewMatrix() const;


    // 뷰포트별 카메라 설정
    void SetupCameraMode();
    void SetViewMode(EViewMode InViewModeIndex) { ViewMode = InViewModeIndex; }

    EViewMode GetViewMode() { return ViewMode;}

    // ===== Pilot 모드 제어 =====
    void EnablePilotMode(AActor* TargetActor, UCameraComponent* TargetCameraComponent);
    void DisablePilotMode();
    bool IsPilotModeEnabled() const { return bPilotCameraMode; }
    AActor* GetPilotActor() const { return PilotActor; }
    UCameraComponent* GetPilotCameraComponent() const { return PilotCameraComponent; }

    // Pilot 모드 이동 속도 (non-ACameraActor용)
    float GetPilotMoveSpeed() const { return PilotMoveSpeed; }
    void SetPilotMoveSpeed(float InSpeed) { PilotMoveSpeed = InSpeed; }

protected:
    void ProcessPilotActorInput(float DeltaTime);
    void SyncCameraWithPilot();  // Pilot 모드에서 PilotCameraComponent를 Camera에 동기화
    EViewportType ViewportType = EViewportType::Perspective;
    UWorld* World = nullptr;
    ACameraActor* Camera = nullptr;
    SViewportWindow* OwnerWindow = nullptr;
    int32 MouseLastX{};
    int32 MouseLastY{};
    bool bIsMouseButtonDown = false;
    bool bIsMouseRightButtonDown = false;

    // 배경색 (기본값: 검은색)
    FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // 언리얼 방식: 모든 직교 뷰포트가 하나의 3D 위치를 공유 (연동되어 움직임)
    static FVector CameraAddPosition;

    // 모든 직교 뷰포트가 공유하는 줌 팩터
    static float SharedOrthographicZoom;

    // 직교 뷰용 카메라 설정
    uint32 OrthographicAddXPosition;
    uint32  OrthographicAddYPosition;
    float OrthographicZoom = 30.0f;
    //뷰모드
    EViewMode ViewMode = EViewMode::VMI_Lit_Phong;

    //원근 투영 기본값
    bool PerspectiveCameraInput = false;
    FVector PerspectiveCameraPosition = FVector(-5.0f, 5.0f, 5.0f);
    FVector PerspectiveCameraRotation = FVector(0.0f, 22.5f, -45.0f);
    float PerspectiveCameraFov=60;

    // ===== Pilot 모드 상태 =====
    bool bPilotCameraMode = false;
    AActor* PilotActor = nullptr;                    // Pilot 대상 액터
    UCameraComponent* PilotCameraComponent = nullptr; // Pilot 대상 카메라 컴포넌트
    float PilotMoveSpeed = 10.0f;                    // Pilot 모드 이동 속도 (non-ACameraActor용)

    // 에디터 카메라 원본 Transform (Pilot 모드 해제 시 복원용)
    // 초기값: 원점에서 떨어져 위에서 원점을 내려다보는 위치
    FVector OriginalCameraLocation = FVector(-5.0f, 5.0f, 5.0f);
    FQuat OriginalCameraRotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 22.5f, -45.0f));
    float OriginalCameraFOV = 60.0f;
    float OriginalCameraNearClip = 0.1f;
    float OriginalCameraFarClip = 2000.0f;
};