#pragma once

#include "Actor.h"
#include "AClothActor.generated.h"

class UClothComponent;
class UClothAsset;

UCLASS(DisplayName = "Cloth Actor", Description = "Simple cloth simulation actor for testing")
class AClothActor : public AActor
{
	GENERATED_REFLECTION_BODY()

public:
	AClothActor();
	~AClothActor() override;

	// Cloth parameters (editable in editor)
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float ClothWidth = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float ClothHeight = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	int32 ResolutionX = 15;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	int32 ResolutionY = 15;

	UPROPERTY(EditAnywhere, Category = "Cloth")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float Damping = 0.8f;  // 높은 damping으로 떨림 방지

	UPROPERTY(EditAnywhere, Category = "Cloth")
	float StiffnessFrequency = 100.0f;  // 매우 높은 강성도 (늘어남 방지)

private:
	void InitializeCloth();

	UClothComponent* ClothComponent = nullptr;
	UClothAsset* ClothAsset = nullptr;
	bool bClothInitialized = false;
};
