# 카메라 Pilot 모드 구현 계획

## 개요

Editor에서 씬에 배치된 UCameraComponent를 가진 액터를 선택하고 해당 카메라 시점에서 직접 이동/회전하며 위치를 조정하는 "Pilot 모드"를 구현합니다.

**장점:**
- Editor에서 카메라 시점을 실시간 확인 가능
- DoF 효과를 카메라 시점에서 직접 테스트 가능
- 원하는 위치에 카메라를 정밀하게 배치 가능
- PIE 모드 없이도 카메라 구도 확인 가능

**지원 대상:**
- ACameraActor (UCameraComponent 보유)
- APlayerCameraManager (UCameraComponent 보유)
- UCameraComponent를 가진 모든 AActor

---

## 진입 방식

뷰포트 드롭다운 메뉴(Perspective/Orthographic 선택)에 씬의 카메라 목록을 추가합니다.

```
[카메라 드롭다운 메뉴]
├─ 원근
│   └─ ● 원근 (Perspective)
├─ 직교
│   └─ ○ 상단 / 하단 / 왼쪽 / 오른쪽 / 정면 / 후면
├─ 카메라 (새 섹션)
│   └─ ○ CameraActor_1
│   └─ ○ PlayerCameraManager_2
│   └─ ...
├─ 이동
│   └─ 카메라 이동 속도 슬라이더
└─ 뷰
    └─ FOV / 근평면 / 원평면 슬라이더
```

---

## 핵심 설계

### Pilot 대상 검색 방식
- `World->GetLevel()->GetActors()`로 모든 액터 순회
- 각 액터의 컴포넌트 중 `UCameraComponent`가 있는지 확인
- 에디터 카메라 제외

### Pilot 모드 제어 방식
- **ACameraActor**: 기존 `ProcessEditorCameraInput()` 사용 (내부적으로 액터 Transform 제어)
- **기타 액터**: 액터의 월드 Transform을 직접 제어

---

## 수정 대상 파일

| 파일 | 역할 |
|------|------|
| `Source/Runtime/Renderer/FViewportClient.h` | Pilot 모드 상태/메서드 선언 |
| `Source/Runtime/Renderer/FViewportClient.cpp` | Pilot 모드 진입/해제/입력 처리 로직 |
| `Source/Slate/Windows/SViewportWindow.cpp` | 드롭다운에 카메라 목록 추가 + Pilot 상태 표시 |

---

## 구현 순서

### Step 1: FViewportClient.h 수정

**파일:** `Source/Runtime/Renderer/FViewportClient.h`

include 및 전방 선언 추가:

```cpp
class UCameraComponent;
```

public 섹션에 다음 메서드 추가:

```cpp
public:
    // Pilot 모드 제어
    void EnablePilotMode(AActor* TargetActor, UCameraComponent* TargetCameraComponent);
    void DisablePilotMode();
    bool IsPilotModeEnabled() const { return bPilotCameraMode; }
    AActor* GetPilotActor() const { return PilotActor; }
    UCameraComponent* GetPilotCameraComponent() const { return PilotCameraComponent; }
```

protected 섹션에 다음 멤버 변수 추가:

```cpp
protected:
    // ===== Pilot 모드 상태 =====
    bool bPilotCameraMode = false;
    AActor* PilotActor = nullptr;                    // Pilot 대상 액터
    UCameraComponent* PilotCameraComponent = nullptr; // Pilot 대상 카메라 컴포넌트
    ACameraActor* OriginalCamera = nullptr;           // 복귀용 에디터 카메라
```

---

### Step 2: FViewportClient.cpp 수정

**파일:** `Source/Runtime/Renderer/FViewportClient.cpp`

include 추가:

```cpp
#include "CameraActor.h"
#include "CameraComponent.h"
```

EnablePilotMode/DisablePilotMode 함수 구현 추가:

```cpp
void FViewportClient::EnablePilotMode(AActor* TargetActor, UCameraComponent* TargetCameraComponent)
{
    if (!TargetActor || !TargetCameraComponent || bPilotCameraMode) return;

    // 현재 에디터 카메라 저장
    OriginalCamera = Camera;

    // Pilot 대상 설정
    PilotActor = TargetActor;
    PilotCameraComponent = TargetCameraComponent;
    bPilotCameraMode = true;

    // ACameraActor인 경우 기존 방식 사용
    if (ACameraActor* CamActor = Cast<ACameraActor>(TargetActor))
    {
        Camera = CamActor;
        CamActor->SyncRotationCache();
    }
}

void FViewportClient::DisablePilotMode()
{
    if (!bPilotCameraMode) return;

    // 에디터 카메라로 복귀
    Camera = OriginalCamera;
    bPilotCameraMode = false;
    PilotActor = nullptr;
    PilotCameraComponent = nullptr;
    OriginalCamera = nullptr;
}
```

---

### Step 3: Tick()에서 Pilot 모드 처리

**파일:** `Source/Runtime/Renderer/FViewportClient.cpp`

Tick() 함수에 Pilot 모드 입력 처리 추가:

```cpp
void FViewportClient::Tick(FViewport* Viewport, float DeltaTime)
{
    // Pilot 모드에서 ESC로 해제
    if (bPilotCameraMode && UInputManager::GetInstance().IsKeyPressed(VK_ESCAPE))
    {
        DisablePilotMode();
        return;
    }

    // Pilot 모드 입력 처리
    if (bPilotCameraMode && PilotActor && bIsMouseRightButtonDown)
    {
        // ACameraActor는 기존 ProcessEditorCameraInput 사용
        if (ACameraActor* CamActor = Cast<ACameraActor>(PilotActor))
        {
            CamActor->ProcessEditorCameraInput(DeltaTime);
        }
        else
        {
            // 일반 액터: 직접 Transform 제어
            ProcessPilotActorInput(DeltaTime);
        }
    }
    // 기존 카메라 입력 처리 (Pilot 모드가 아닐 때만)
    else if (!bPilotCameraMode && PerspectiveCameraInput)
    {
        Camera->ProcessEditorCameraInput(DeltaTime);
    }

    // ... 기존 코드 ...
}
```

---

### Step 4: ProcessPilotActorInput() 함수 추가

**파일:** `Source/Runtime/Renderer/FViewportClient.cpp`

일반 액터의 Transform을 직접 제어하는 함수 추가:

```cpp
void FViewportClient::ProcessPilotActorInput(float DeltaTime)
{
    if (!PilotActor) return;

    UInputManager& Input = UInputManager::GetInstance();

    // 현재 Transform 가져오기
    FVector Location = PilotActor->GetActorLocation();
    FQuat Rotation = PilotActor->GetActorRotation();

    // 마우스 델타로 회전 (Yaw/Pitch)
    float MouseDeltaX = Input.GetMouseDeltaX();
    float MouseDeltaY = Input.GetMouseDeltaY();

    // 현재 회전을 오일러로 변환
    FVector EulerAngles = Rotation.ToEulerAngles();
    float Yaw = EulerAngles.Z;
    float Pitch = EulerAngles.X;

    // 마우스 감도 적용
    const float MouseSensitivity = 0.1f;
    Yaw += MouseDeltaX * MouseSensitivity;
    Pitch += MouseDeltaY * MouseSensitivity;

    // Pitch 제한 (-89 ~ 89도)
    Pitch = FMath::Clamp(Pitch, -89.0f, 89.0f);

    // 새 회전 적용
    Rotation = FQuat::FromEulerAngles(FVector(Pitch, 0.0f, Yaw));

    // WASD/QE 이동
    FVector MoveDirection = FVector::ZeroVector;
    const float MoveSpeed = 5.0f; // 기본 이동 속도

    if (Input.IsKeyDown('W')) MoveDirection += Rotation.GetForwardVector();
    if (Input.IsKeyDown('S')) MoveDirection -= Rotation.GetForwardVector();
    if (Input.IsKeyDown('D')) MoveDirection += Rotation.GetRightVector();
    if (Input.IsKeyDown('A')) MoveDirection -= Rotation.GetRightVector();
    if (Input.IsKeyDown('E')) MoveDirection += FVector::UpVector;
    if (Input.IsKeyDown('Q')) MoveDirection -= FVector::UpVector;

    if (MoveDirection.LengthSquared() > 0.0f)
    {
        MoveDirection.Normalize();
        Location += MoveDirection * MoveSpeed * DeltaTime;
    }

    // Transform 적용
    PilotActor->SetActorLocation(Location);
    PilotActor->SetActorRotation(Rotation);
}
```

**파일:** `Source/Runtime/Renderer/FViewportClient.h`

protected 섹션에 함수 선언 추가:

```cpp
protected:
    void ProcessPilotActorInput(float DeltaTime);
```

---

### Step 5: SViewportWindow.cpp 드롭다운 메뉴 수정

**파일:** `Source/Slate/Windows/SViewportWindow.cpp`

include 추가 (파일 상단):

```cpp
#include "CameraComponent.h"
#include "World.h"
#include "Level.h"
```

`RenderCameraOptionDropdownMenu()` 함수에서 직교 모드 목록(line 868) 이후, "이동" 섹션(line 870) 이전에 카메라 섹션 추가:

```cpp
        // ... 직교 모드 for loop 끝 (line 868) ...

        // --- 섹션: 카메라 (Pilot 모드) ---
        if (World)
        {
            // UCameraComponent를 가진 모든 액터 검색
            struct FPilotableCamera
            {
                AActor* Actor;
                UCameraComponent* CameraComponent;
            };
            TArray<FPilotableCamera> PilotableCameras;

            for (AActor* Actor : World->GetLevel()->GetActors())
            {
                if (!Actor || Actor->IsPendingDestroy()) continue;

                // 에디터 카메라 제외
                if (ViewportClient && Actor == ViewportClient->GetCamera()) continue;

                // UCameraComponent 검색
                for (UActorComponent* Comp : Actor->GetComponents())
                {
                    if (UCameraComponent* CamComp = Cast<UCameraComponent>(Comp))
                    {
                        PilotableCameras.Add({ Actor, CamComp });
                        break; // 액터당 하나의 카메라만
                    }
                }
            }

            if (PilotableCameras.Num() > 0)
            {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "카메라");
                ImGui::Separator();

                for (int32 i = 0; i < PilotableCameras.Num(); i++)
                {
                    const FPilotableCamera& PilotCam = PilotableCameras[i];
                    bool bIsPiloting = ViewportClient && ViewportClient->IsPilotModeEnabled() &&
                                       ViewportClient->GetPilotActor() == PilotCam.Actor;
                    const char* RadioIcon = bIsPiloting ? "●" : "○";

                    char SelectableID[64];
                    sprintf_s(SelectableID, "##Camera%d", i);

                    ImVec2 CamSelectableCursorPos = ImGui::GetCursorPos();

                    if (ImGui::Selectable(SelectableID, bIsPiloting, 0, SelectableSize))
                    {
                        if (bIsPiloting)
                        {
                            // 이미 Pilot 중이면 해제
                            ViewportClient->DisablePilotMode();
                            ViewportType = EViewportType::Perspective;
                            ViewportName = "원근";
                            ViewportClient->SetViewportType(ViewportType);
                            ViewportClient->SetupCameraMode();
                        }
                        else
                        {
                            // Pilot 모드 진입
                            ViewportClient->EnablePilotMode(PilotCam.Actor, PilotCam.CameraComponent);
                            ViewportName = PilotCam.Actor->GetName().c_str();
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("이 카메라 시점으로 전환합니다 (Pilot 모드)");
                    }

                    // 라디오 아이콘 + 카메라 이름 렌더링
                    ImVec2 CamContentPos = ImVec2(CamSelectableCursorPos.x + 4,
                        CamSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
                    ImGui::SetCursorPos(CamContentPos);

                    ImGui::Text("%s", RadioIcon);
                    ImGui::SameLine(0, 4);

                    // 카메라 아이콘
                    if (IconCamera && IconCamera->GetShaderResourceView())
                    {
                        ImGui::Image((void*)IconCamera->GetShaderResourceView(), IconSize);
                        ImGui::SameLine(0, 4);
                    }

                    ImGui::Text("%s", PilotCam.Actor->GetName().c_str());
                }
            }
        }

        // --- 섹션 3: 이동 --- (기존 코드, line 870)
```

---

### Step 6: 드롭다운 버튼 텍스트 업데이트

**파일:** `Source/Slate/Windows/SViewportWindow.cpp`

`RenderCameraOptionDropdownMenu()` 함수 시작 부분 (line 670 부근)에서 ButtonText 설정 후 Pilot 모드 체크 추가:

```cpp
void SViewportWindow::RenderCameraOptionDropdownMenu()
{
    // ... 기존 cursorPos 설정 ...

    const char* ButtonText = ViewportName.c_str();

    // Pilot 모드일 때 액터 이름 표시
    if (ViewportClient && ViewportClient->IsPilotModeEnabled() && ViewportClient->GetPilotActor())
    {
        ButtonText = ViewportClient->GetPilotActor()->GetName().c_str();
    }

    // ... 기존 코드 계속 ...
}
```

---

### Step 7: (선택) 도구모음에 Pilot 상태 표시

**파일:** `Source/Slate/Windows/SViewportWindow.cpp`

`RenderToolbar()` 함수 끝 부분에 Pilot 모드 상태 표시 추가:

```cpp
void SViewportWindow::RenderToolbar()
{
    // ... 기존 코드 ...

    // Pilot 모드 상태 표시
    if (ViewportClient && ViewportClient->IsPilotModeEnabled())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Pilot]");

        ImGui::SameLine();
        if (ImGui::SmallButton("Stop"))
        {
            ViewportClient->DisablePilotMode();
            ViewportType = EViewportType::Perspective;
            ViewportName = "원근";
            ViewportClient->SetViewportType(ViewportType);
            ViewportClient->SetupCameraMode();
        }
    }
}
```

---

### Step 8: Draw()에서 Pilot 카메라 뷰 사용

**파일:** `Source/Runtime/Renderer/FViewportClient.cpp`

Draw() 함수에서 Pilot 모드일 때 PilotCameraComponent의 뷰 매트릭스 사용:

```cpp
void FViewportClient::Draw(FViewport* Viewport)
{
    // Pilot 모드일 때 카메라 컴포넌트의 뷰 사용
    if (bPilotCameraMode && PilotCameraComponent)
    {
        // PilotCameraComponent의 뷰/프로젝션 매트릭스 사용
        // (구체적인 구현은 기존 Draw 로직에 맞춰 조정)
    }

    // ... 기존 코드 ...
}
```

---

## 조작법

| 입력 | 동작 |
|------|------|
| 뷰포트 드롭다운 → 카메라 섹션에서 카메라 선택 | Pilot 모드 진입 |
| 우클릭 + 마우스 이동 | 카메라(액터) 회전 |
| 우클릭 + W/S/A/D | 앞/뒤/좌/우 이동 |
| 우클릭 + Q/E | 아래/위 이동 |
| ESC | Pilot 모드 해제 |
| 드롭다운에서 "원근" 선택 | Pilot 모드 해제 + 원근 모드 |
| 도구모음 "Stop" 버튼 | Pilot 모드 해제 |

---

## 빌드 및 테스트

1. 빌드 실행
2. Editor 실행
3. 씬에 ACameraActor 또는 APlayerCameraManager 배치
4. 뷰포트 드롭다운에서 해당 카메라 선택
5. 카메라 시점으로 전환되는지 확인
6. 우클릭 + WASD로 카메라(액터) 이동 가능한지 확인
7. 액터의 월드 Transform이 변경되는지 확인
8. ESC 또는 "원근" 선택으로 Pilot 모드 해제 확인
