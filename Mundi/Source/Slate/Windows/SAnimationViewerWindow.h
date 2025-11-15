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

    virtual void PreRenderViewportUpdate() override;

    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(const FString& Path);

protected:
    virtual ViewerState* CreateViewerState(const char* Name) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
};