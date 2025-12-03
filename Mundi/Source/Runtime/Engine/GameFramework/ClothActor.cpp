#include "pch.h"
#include "ClothActor.h"
#include "ClothComponent.h"
#include "ClothAsset.h"
#include "ClothCore.h"

AClothActor::AClothActor()
{
	// Create ClothComponent as default subobject
	ClothComponent = CreateDefaultSubobject<UClothComponent>("ClothComponent");
	if (ClothComponent)
	{
		SetRootComponent(ClothComponent);
	}

	// Initialize cloth in constructor (for immediate editor feedback)
	InitializeCloth();
}

void AClothActor::InitializeCloth()
{
	UE_LOG("========================================\n");
	UE_LOG("[ClothActor] Initializing cloth for editor rendering\n");

	// Check if ClothComponent exists
	if (!ClothComponent)
	{
		UE_LOG("[ClothActor] ERROR: ClothComponent is null.\n");
		UE_LOG("========================================\n");
		return;
	}

	UE_LOG("[ClothActor] ClothComponent exists: OK\n");
	UE_LOG("[ClothActor] Creating cloth mesh...\n");

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
