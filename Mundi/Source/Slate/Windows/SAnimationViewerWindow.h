#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;

class SAnimationViewerWindow : public SViewerWindow
{
public:
    SAnimationViewerWindow();
    virtual ~SAnimationViewerWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    void OnRenderViewport();

    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(const FString& Path);

private:
    // Tabs
    void OpenNewTab(const char* Name = "Viewer");
    void CloseTab(int Index);

private:
    // For legacy single-state flows; removed once tabs are stable
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    // Layout state
    float LeftPanelRatio = 0.25f;   // 25% of width
    float RightPanelRatio = 0.25f;  // 25% of width

public:
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
};