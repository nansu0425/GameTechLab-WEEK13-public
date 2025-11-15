#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;

class SSkeletalMeshViewerWindow : public SViewerWindow
{
public:
    SSkeletalMeshViewerWindow();
    virtual ~SSkeletalMeshViewerWindow();

    virtual void PreRenderViewportUpdate() override;

    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(const FString& Path);

protected:
    virtual ViewerState* CreateViewerState(const char* Name) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
};