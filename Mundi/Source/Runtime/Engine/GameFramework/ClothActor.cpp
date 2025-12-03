#include "pch.h"
#include "ClothActor.h"
#include "ClothComponent.h"
#include "ClothAsset.h"
#include "ClothCore.h"
#include "LineComponent.h"
#include "Line.h"
#include <NvCloth/Cloth.h>
#include <NvCloth/Range.h>

AClothActor::AClothActor()
{
	// Create ClothComponent as default subobject
	ClothComponent = CreateDefaultSubobject<UClothComponent>("ClothComponent");
	if (ClothComponent)
	{
		SetRootComponent(ClothComponent);
	}

	// Create LineComponent for visualization
	LineComponent = CreateDefaultSubobject<ULineComponent>("LineComponent");
	if (LineComponent)
	{
		LineComponent->SetupAttachment(ClothComponent, EAttachmentRule::KeepRelative);
		LineComponent->SetAlwaysOnTop(false);
	}

	// Initialize cloth in constructor (for immediate editor feedback)
	InitializeCloth();
}

void AClothActor::InitializeCloth()
{
	UE_LOG("========================================\n");
	UE_LOG("[ClothActor] Initializing cloth (Constructor/Editor)\n");

	// Check if ClothCore is initialized
	if (!FClothCore::GetInstance().IsInitialized())
	{
		UE_LOG("[ClothActor] ClothCore not initialized yet. Will initialize in BeginPlay.\n");
		UE_LOG("========================================\n");
		return;
	}

	UE_LOG("[ClothActor] ClothCore is initialized: OK\n");

	// Check if ClothComponent exists
	if (!ClothComponent)
	{
		UE_LOG("[ClothActor] ERROR: ClothComponent is null.\n");
		UE_LOG("========================================\n");
		return;
	}

	UE_LOG("[ClothActor] ClothComponent exists: OK\n");
	UE_LOG("[ClothActor] Creating cloth simulation...\n");

	// Create plane cloth asset
	ClothAsset = UClothAsset::CreatePlaneCloth(ClothWidth, ClothHeight, ResolutionX, ResolutionY);
	if (!ClothAsset)
	{
		UE_LOG("[ClothActor] ERROR: Failed to create cloth asset.\n");
		UE_LOG("========================================\n");
		return;
	}

	// Assign asset to component
	ClothComponent->SetClothAsset(ClothAsset);

	// Set simulation parameters
	ClothComponent->Gravity = Gravity;
	ClothComponent->Damping = Damping;
	ClothComponent->StiffnessFrequency = StiffnessFrequency;
	ClothComponent->bEnableClothSimulation = true;

	UE_LOG("[ClothActor] Cloth configured successfully!\n");
	UE_LOG("[ClothActor] - Size: %.1fx%.1f\n", ClothWidth, ClothHeight);
	UE_LOG("[ClothActor] - Resolution: %dx%d (%d particles)\n",
		ResolutionX, ResolutionY, ClothAsset->GetParticleCount());
	UE_LOG("========================================\n");

	bClothInitialized = true;
}

AClothActor::~AClothActor()
{
}

void AClothActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG("========================================\n");
	UE_LOG("[ClothActor] BeginPlay called (PIE)!\n");
	UE_LOG("[ClothActor] Actor location: (%.1f, %.1f, %.1f)\n",
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);

	// If cloth wasn't initialized in constructor (ClothCore wasn't ready), initialize now
	if (!bClothInitialized)
	{
		UE_LOG("[ClothActor] Cloth not initialized yet, initializing now...\n");
		InitializeCloth();
	}
	else
	{
		UE_LOG("[ClothActor] Cloth already initialized in constructor.\n");
	}

	UE_LOG("========================================\n");
}

void AClothActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update cloth visualization
	if (bClothInitialized && ClothComponent && LineComponent)
	{
		UpdateVisualization();
	}
}

void AClothActor::InitializeLineCache()
{
	if (!ClothAsset || !LineComponent || bLineCacheInitialized)
		return;

	const TArray<uint32>& Indices = ClothAsset->GetTriangleIndices();
	FVector4 LineColor(0.0f, 1.0f, 0.5f, 1.0f);

	// Build unique edge list (once!)
	std::set<std::pair<uint32, uint32>> UniqueEdgesSet;
	for (size_t i = 0; i < Indices.size(); i += 3)
	{
		uint32 i0 = Indices[i];
		uint32 i1 = Indices[i + 1];
		uint32 i2 = Indices[i + 2];

		auto AddEdge = [&](uint32 a, uint32 b) {
			if (a > b) std::swap(a, b);
			UniqueEdgesSet.insert({a, b});
		};

		AddEdge(i0, i1);
		AddEdge(i1, i2);
		AddEdge(i2, i0);
	}

	// Convert to array for fast iteration
	UniqueEdgeList.clear();
	UniqueEdgeList.reserve(UniqueEdgesSet.size());
	for (const auto& Edge : UniqueEdgesSet)
	{
		UniqueEdgeList.push_back(Edge);
	}

	// Pre-create ULine objects
	LineComponent->ClearLines();
	CachedLines.clear();
	CachedLines.reserve(UniqueEdgeList.size());

	for (const auto& Edge : UniqueEdgeList)
	{
		ULine* Line = LineComponent->AddLine(FVector::Zero(), FVector::Zero(), LineColor);
		CachedLines.push_back(Line);
	}

	bLineCacheInitialized = true;
	UE_LOG("[ClothActor] Line cache initialized: %d unique edges\n", CachedLines.size());
}

void AClothActor::UpdateVisualization()
{
	if (!ClothComponent || !ClothAsset || !LineComponent)
		return;

	nv::cloth::Cloth* ClothInstance = ClothComponent->GetClothInstance();
	if (!ClothInstance)
		return;

	// Initialize line cache on first frame
	if (!bLineCacheInitialized)
	{
		InitializeLineCache();
		return;
	}

	// Get current particle positions
	nv::cloth::MappedRange<physx::PxVec4> Particles = ClothInstance->getCurrentParticles();
	if (Particles.size() == 0)
		return;

	// Update line positions using pre-computed edge list (FAST!)
	for (size_t i = 0; i < UniqueEdgeList.size(); ++i)
	{
		const auto& Edge = UniqueEdgeList[i];
		uint32 a = Edge.first;
		uint32 b = Edge.second;

		if (a < Particles.size() && b < Particles.size())
		{
			FVector p0(Particles[a].x, Particles[a].y, Particles[a].z);
			FVector p1(Particles[b].x, Particles[b].y, Particles[b].z);
			CachedLines[i]->SetLine(p0, p1);
		}
	}
}
