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
	// Fabric 해제
	if (CachedFabric)
	{
		CachedFabric->decRefCount();
		CachedFabric = nullptr;
	}
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

nv::cloth::Fabric* UClothAsset::GetOrCreateFabric(nv::cloth::Factory* Factory)
{
	if (!Factory)
		return nullptr;

	// 캐시된 Fabric이 있고 유효하면 반환
	if (bFabricValid && CachedFabric)
		return CachedFabric;

	// 메쉬 데이터가 없으면 실패
	if (ParticlePositions.empty() || TriangleIndices.empty())
	{
		printf("[ClothAsset] ERROR: No mesh data to cook fabric.\n");
		return nullptr;
	}

	// 기존 Fabric 해제
	if (CachedFabric)
	{
		CachedFabric->decRefCount();
		CachedFabric = nullptr;
	}

	uint32_t NumParticles = (uint32_t)ParticlePositions.size();

	// Edge 추출 (중복 제거)
	std::set<std::pair<uint32_t, uint32_t>> EdgeSet;
	for (size_t i = 0; i < TriangleIndices.size(); i += 3)
	{
		uint32_t i0 = TriangleIndices[i];
		uint32_t i1 = TriangleIndices[i + 1];
		uint32_t i2 = TriangleIndices[i + 2];

		auto AddEdge = [&](uint32_t a, uint32_t b) {
			if (a > b) std::swap(a, b);
			EdgeSet.insert({a, b});
		};

		AddEdge(i0, i1);
		AddEdge(i1, i2);
		AddEdge(i2, i0);
	}

	// Constraint 데이터 구성
	TArray<uint32_t> PhaseIndices;
	TArray<uint32_t> Sets;
	TArray<float> RestValues;
	TArray<float> StiffnessValues;
	TArray<uint32_t> ConstraintIndices;

	PhaseIndices.push_back(0); // Single phase
	Sets.push_back(0);
	Sets.push_back((uint32_t)EdgeSet.size());

	// Tether constraint 데이터 (고정점으로부터 최대 거리 제한)
	TArray<uint32_t> TetherAnchors;
	TArray<float> TetherLengths;

	// 고정점 찾기 (invMass == 0인 particle)
	TArray<uint32_t> FixedParticles;
	for (uint32_t i = 0; i < NumParticles; ++i)
	{
		if (ParticlePositions[i].W == 0.0f)  // invMass == 0 → 고정점
		{
			FixedParticles.push_back(i);
		}
	}

	// 각 particle에 대해 가장 가까운 고정점과의 거리 계산
	for (uint32_t i = 0; i < NumParticles; ++i)
	{
		// 이미 고정점이면 tether 불필요
		if (ParticlePositions[i].W == 0.0f)
			continue;

		// 가장 가까운 고정점 찾기
		float MinDistance = FLT_MAX;
		uint32_t NearestAnchor = 0;

		for (uint32_t AnchorIdx : FixedParticles)
		{
			FVector4 p0 = ParticlePositions[i];
			FVector4 p1 = ParticlePositions[AnchorIdx];
			float Distance = sqrtf(
				(p1.X - p0.X) * (p1.X - p0.X) +
				(p1.Y - p0.Y) * (p1.Y - p0.Y) +
				(p1.Z - p0.Z) * (p1.Z - p0.Z)
			);

			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				NearestAnchor = AnchorIdx;
			}
		}

		// Tether 추가
		TetherAnchors.push_back(NearestAnchor);
		TetherLengths.push_back(MinDistance * 1.1f);  // 10% 여유
	}

	// Edge마다 constraint 생성
	for (const auto& Edge : EdgeSet)
	{
		uint32_t i0 = Edge.first;
		uint32_t i1 = Edge.second;

		// Rest length 계산
		FVector4 p0 = ParticlePositions[i0];
		FVector4 p1 = ParticlePositions[i1];
		float RestLength = sqrtf(
			(p1.X - p0.X) * (p1.X - p0.X) +
			(p1.Y - p0.Y) * (p1.Y - p0.Y) +
			(p1.Z - p0.Z) * (p1.Z - p0.Z)
		);

		RestValues.push_back(RestLength);
		StiffnessValues.push_back(1.0f); // Full stiffness
		ConstraintIndices.push_back(i0);
		ConstraintIndices.push_back(i1);
	}

	// Fabric 생성
	CachedFabric = Factory->createFabric(
		NumParticles,
		nv::cloth::Range<const uint32_t>(PhaseIndices.data(), PhaseIndices.data() + PhaseIndices.size()),
		nv::cloth::Range<const uint32_t>(Sets.data(), Sets.data() + Sets.size()),
		nv::cloth::Range<const float>(RestValues.data(), RestValues.data() + RestValues.size()),
		nv::cloth::Range<const float>(StiffnessValues.data(), StiffnessValues.data() + StiffnessValues.size()),
		nv::cloth::Range<const uint32_t>(ConstraintIndices.data(), ConstraintIndices.data() + ConstraintIndices.size()),
		nv::cloth::Range<const uint32_t>(TetherAnchors.data(), TetherAnchors.data() + TetherAnchors.size()),
		nv::cloth::Range<const float>(TetherLengths.data(), TetherLengths.data() + TetherLengths.size()),
		nv::cloth::Range<const uint32_t>()
	);

	if (!CachedFabric)
	{
		printf("[ClothAsset] ERROR: Failed to create fabric.\n");
		return nullptr;
	}

	bFabricValid = true;
	printf("[ClothAsset] Fabric created successfully. Particles: %d, Constraints: %d, Tethers: %d, Fixed: %d\n",
		NumParticles, (int)EdgeSet.size(), (int)TetherAnchors.size(), (int)FixedParticles.size());

	return CachedFabric;
}

void UClothAsset::SetMeshData(const TArray<FVector4>& InPositions, const TArray<uint32>& InIndices)
{
	ParticlePositions = InPositions;
	TriangleIndices = InIndices;
	InvalidateFabric();
}

void UClothAsset::InvalidateFabric()
{
	bFabricValid = false;

	if (CachedFabric)
	{
		CachedFabric->decRefCount();
		CachedFabric = nullptr;
	}
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

			// Fix the entire top edge to prevent excessive curling
			// This provides more stability while still allowing the cloth to wave
			bool bIsTopEdge = (y == ResolutionY - 1);
			float InvMass = bIsTopEdge ? 0.0f : 1.0f;

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
