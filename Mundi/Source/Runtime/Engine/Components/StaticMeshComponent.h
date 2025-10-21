#pragma once
#include "MeshComponent.h"
#include "Enums.h"
#include "AABB.h"

class UStaticMesh;
class UShader;
class UTexture;
struct FSceneCompData;

class UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
	GENERATED_REFLECTION_BODY()

	UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;

public:
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	
	UMaterial* GetMaterial(uint32 InSectionIndex) const override;
	void SetMaterial(uint32 InElementIndex, UMaterial* InNewMaterial) override;

	void SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName);
	const TArray<UMaterial*> GetMaterialSlots() const { return MaterialSlots; }

	bool IsChangedMaterialByUser() const
	{
		return bChangedMaterialByUser;
	}

	FAABB GetWorldAABB() const;

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UStaticMeshComponent)

protected:
	void OnTransformUpdated() override;
	void MarkWorldPartitionDirty();

protected:
	UStaticMesh* StaticMesh = nullptr;
	TArray<UMaterial*> MaterialSlots;

	bool bChangedMaterialByUser = false;
};
