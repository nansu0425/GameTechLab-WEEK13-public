#pragma once
#include "Object.h"

class USkeletalMesh;
class SWindow;
class UEditorAssetPreviewContext : public UObject
{
public:
	UEditorAssetPreviewContext();

	USkeletalMesh* SkeletalMesh;
	TArray<SWindow*> ListeningWindows;
	EViewerType ViewerType;
	FString AssetPath;
};