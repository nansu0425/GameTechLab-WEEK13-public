#pragma once

#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"   
#include "Vector.h"
#include "UEContainer.h"
using namespace json;

struct FSceneCompData
{
    uint32 UUID = 0;
    FVector Location;
    FVector Rotation;
    FVector Scale;
    FString Type;
    //FString ObjStaticMeshAsset;
};

struct FBillboardData
{
    float width;
    float height;
    FString TexturePath;
};

struct FStaticMeshCompData
{
    FString ObjStaticMeshAsset;
};

struct FPerspectiveCameraData
{
    FVector Location;
	FVector Rotation;
	float FOV;
	float NearClip;
	float FarClip;
};

class FSceneLoader
{
public:
    static TArray<FSceneCompData> Load(const FString& FileName, FPerspectiveCameraData* OutCameraData);
    // 중복 I/O 방지 NextUUID와 함께 로드
    static TArray<FSceneCompData> LoadWithUUID(const FString& FileName, FPerspectiveCameraData& OutCameraData, uint32& OutNextUUID);
    static void Save(TArray<FSceneCompData> InPrimitiveData, const FPerspectiveCameraData* InCameraData, const FString& SceneName);
    static bool TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID);

private:
    static TArray<FSceneCompData> Parse(const JSON& Json);
    static TArray<FSceneCompData> ParseSinglePass(const JSON& Json, FPerspectiveCameraData* OutCameraData);
};