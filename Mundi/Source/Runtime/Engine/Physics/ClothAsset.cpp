#include "pch.h"
#include "ClothAsset.h"
#include "ClothCore.h"
#include <NvCloth/Fabric.h>
#include <NvCloth/Factory.h>

UClothAsset::UClothAsset()
{
}

UClothAsset::~UClothAsset()
{
}

void UClothAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// Particle 위치 로드
		if (InOutHandle.hasKey("ParticlePositions"))
		{
			auto& PosArray = InOutHandle["ParticlePositions"];
			ParticlePositions.clear();
			for (auto& PosObj : PosArray.ArrayRange())
			{
				FVector4 Pos;
				Pos.X = PosObj["x"].ToFloat();
				Pos.Y = PosObj["y"].ToFloat();
				Pos.Z = PosObj["z"].ToFloat();
				Pos.W = PosObj["w"].ToFloat();
				ParticlePositions.push_back(Pos);
			}
		}

		// Triangle 인덱스 로드
		if (InOutHandle.hasKey("TriangleIndices"))
		{
			auto& IdxArray = InOutHandle["TriangleIndices"];
			TriangleIndices.clear();
			for (auto& IdxObj : IdxArray.ArrayRange())
			{
				TriangleIndices.push_back(IdxObj.ToInt());
			}
		}
	}
	else
	{
		// Particle 위치 저장
		JSON PosArray = JSON::Make(JSON::Class::Array);
		for (const FVector4& Pos : ParticlePositions)
		{
			JSON PosObj = JSON::Make(JSON::Class::Object);
			PosObj["x"] = Pos.X;
			PosObj["y"] = Pos.Y;
			PosObj["z"] = Pos.Z;
			PosObj["w"] = Pos.W;
			PosArray.append(PosObj);
		}
		InOutHandle["ParticlePositions"] = PosArray;

		// Triangle 인덱스 저장
		JSON IdxArray = JSON::Make(JSON::Class::Array);
		for (uint32 Idx : TriangleIndices)
		{
			IdxArray.append((int)Idx);
		}
		InOutHandle["TriangleIndices"] = IdxArray;
	}
}

void UClothAsset::SetMeshData(const TArray<FVector4>& InPositions, const TArray<uint32>& InIndices)
{
	ParticlePositions = InPositions;
	TriangleIndices = InIndices;
}

UClothAsset* UClothAsset::CreatePlaneCloth(float Width, float Height, int32 ResolutionX, int32 ResolutionY)
{
	// Validation
	if (ResolutionX < 2 || ResolutionY < 2)
	{
		printf("[ClothAsset] ERROR: Resolution must be at least 2x2.\n");
		return nullptr;
	}

	UClothAsset* NewAsset = NewObject<UClothAsset>();
	if (!NewAsset)
	{
		printf("[ClothAsset] ERROR: Failed to create ClothAsset object.\n");
		return nullptr;
	}

	// Generate grid vertices
	TArray<FVector4> Positions;
	TArray<uint32> Indices;

	float StepX = Width / (ResolutionX - 1);
	float StepY = Height / (ResolutionY - 1);

	// Create vertices (XZ plane, vertical curtain)
	// Origin at bottom-left corner
	for (int32 y = 0; y < ResolutionY; ++y)
	{
		for (int32 x = 0; x < ResolutionX; ++x)
		{
			float PosX = x * StepX; // Start from 0 (left edge)
			float PosY = 0.0f; // Flat in Y direction
			float PosZ = y * StepY; // Height goes up in Z from 0 (bottom)

			// W component is just stored but not used for physics
			float InvMass = 1.0f;

			Positions.emplace_back(PosX, PosY, PosZ, InvMass);
		}
	}

	// Create triangles
	for (int32 y = 0; y < ResolutionY - 1; ++y)
	{
		for (int32 x = 0; x < ResolutionX - 1; ++x)
		{
			uint32 i0 = y * ResolutionX + x;
			uint32 i1 = i0 + 1;
			uint32 i2 = i0 + ResolutionX;
			uint32 i3 = i2 + 1;

			// Triangle 1
			Indices.push_back(i0);
			Indices.push_back(i2);
			Indices.push_back(i1);

			// Triangle 2
			Indices.push_back(i1);
			Indices.push_back(i2);
			Indices.push_back(i3);
		}
	}

	NewAsset->SetMeshData(Positions, Indices);

	printf("[ClothAsset] Created plane cloth: %dx%d resolution, %d vertices, %d triangles\n",
		ResolutionX, ResolutionY, Positions.size(), Indices.size() / 3);

	return NewAsset;
}
