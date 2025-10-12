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
    static bool TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID);

private:
};