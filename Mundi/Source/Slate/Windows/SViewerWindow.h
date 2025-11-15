#pragma once
#include "SWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class SViewerWindow : public SWindow
{
public:
	SViewerWindow();
	virtual ~SViewerWindow();

	// SWindow overrides
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// Accessors (active tab)
	FViewport* GetViewport() const { return ActiveState ? ActiveState->Viewport : nullptr; }
	FViewportClient* GetViewportClient() const { return ActiveState ? ActiveState->Client : nullptr; }

protected:
	// Per-tab state
	ViewerState* ActiveState = nullptr;
	TArray<ViewerState*> Tabs;
	int ActiveTabIndex = -1;

	// Cached center region used for viewport sizing and input mapping
	FRect CenterRect;

	// Whether we've applied the initial ImGui window placement
	bool bInitialPlacementDone = false;

	// Request focus on first open
	bool bRequestFocus = false;

	// Window open state
	bool bIsOpen = true;
	
	void UpdateBoneTransformFromSkeleton(ViewerState* State);
	void ApplyBoneTransform(ViewerState* State);
	void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);
};