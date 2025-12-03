#pragma once

#include "Actor.h"
#include "AClothActor.generated.h"

class UClothComponent;
class UClothAsset;
class ULineComponent;

UCLASS(DisplayName = "Cloth Actor", Description = "Simple cloth simulation actor for testing")
class AClothActor : public AActor
{
	GENERATED_REFLECTION_BODY()

public:
	AClothActor();
	~AClothActor() override;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	// Cloth parameters (editable in editor)
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float ClothWidth = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float ClothHeight = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	int32 ResolutionX = 15;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	int32 ResolutionY = 15;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float Damping = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float StiffnessFrequency = 10.0f;

private:
	void InitializeCloth();
	void UpdateVisualization();
	void InitializeLineCache();

	UClothComponent* ClothComponent = nullptr;
	UClothAsset* ClothAsset = nullptr;
	ULineComponent* LineComponent = nullptr;
	bool bClothInitialized = false;

	// Line cache for performance (avoid recreating ULine objects every frame)
	TArray<class ULine*> CachedLines;
	TArray<std::pair<uint32, uint32>> UniqueEdgeList; // Pre-computed edge list
	bool bLineCacheInitialized = false;
};
