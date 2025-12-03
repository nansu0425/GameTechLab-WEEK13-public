#pragma once

#include "PrimitiveComponent.h"
#include "UClothComponent.generated.h"

// 전방 선언
class UClothAsset;
class UMeshComponent;
class UDynamicMesh;
class UMaterial;

namespace nv
{
	namespace cloth
	{
		class Cloth;
		class Fabric;
	}
}

namespace physx
{
	struct PxVec4;
}

/**
 * @brief Cloth 시뮬레이션 컴포넌트
 *
 * USkeletalMeshComponent의 래그돌 시스템과 유사하게 동작하는 Cloth 전용 컴포넌트.
 * ClothAsset을 참조하여 NvCloth 시뮬레이션을 실행하고 결과를 렌더링 메쉬에 반영.
 */
UCLASS(DisplayName = "Cloth 컴포넌트", Description = "Cloth 시뮬레이션 컴포넌트")
class UClothComponent : public UPrimitiveComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UClothComponent();
	~UClothComponent() override;

	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;
	void TickComponent(float DeltaTime) override;

	// Rendering
	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	// ═══════════════════════════════════════════════════════════════════════
	// Cloth Asset
	// ═══════════════════════════════════════════════════════════════════════

	/** Cloth 애셋 설정 */
	void SetClothAsset(UClothAsset* InAsset);

	/** Cloth 애셋 접근 */
	UClothAsset* GetClothAsset() const { return ClothAsset; }

	// ═══════════════════════════════════════════════════════════════════════
	// Simulation Parameters (USkeletalMeshComponent의 Physics 프로퍼티와 유사)
	// ═══════════════════════════════════════════════════════════════════════

	/** Cloth 시뮬레이션 활성화 여부 */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	bool bEnableClothSimulation = true;

	/** 중력 벡터 */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	/** 바람 벡터 (외력) */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	FVector Wind = FVector(500.0f, 0.0f, 0.0f);

	/** 바람 적용 여부 */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	bool bEnableWind = true;

	/** Damping (0~1, 높을수록 움직임 감소) */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float Damping = 0.2f;

	/** Linear drag (공기 저항) */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float LinearDrag = 0.1f;

	/** Angular drag (회전 저항) */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float AngularDrag = 0.1f;

	/** Stiffness frequency (constraint 강성도, 높을수록 단단함) */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float StiffnessFrequency = 10.0f;

	/** Self collision 활성화 여부 */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	bool bEnableSelfCollision = false;

	/** Self collision 거리 */
	UPROPERTY(EditAnywhere, Category = "Cloth")
	float SelfCollisionDistance = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════
	// Cloth Physics State
	// ═══════════════════════════════════════════════════════════════════════

	/** Cloth 물리 상태 생성 */
	void CreateClothPhysicsState();

	/** Cloth 물리 상태 파괴 */
	void DestroyClothPhysicsState();

	/** Cloth 시뮬레이션 활성화/비활성화 */
	void SetEnableClothSimulation(bool bEnable);

	/** Cloth 시뮬레이션 중인지 */
	bool IsSimulatingCloth() const { return bSimulatingCloth; }

	/** NvCloth 인스턴스 접근 (내부용) */
	nv::cloth::Cloth* GetClothInstance() const { return ClothInstance; }

	// ═══════════════════════════════════════════════════════════════════════
	// Rendering Integration
	// ═══════════════════════════════════════════════════════════════════════

	/** 렌더링 메쉬 컴포넌트 설정 (동적 메쉬 업데이트용) */
	void SetRenderMeshComponent(UMeshComponent* InMeshComponent);

	/** 렌더링 메쉬 컴포넌트 접근 */
	UMeshComponent* GetRenderMeshComponent() const { return RenderMeshComponent; }

	/** Get dynamic mesh for rendering */
	UDynamicMesh* GetDynamicMesh() const { return DynamicMesh; }

	/** Set material for cloth rendering */
	void SetClothMaterial(UMaterial* InMaterial);

protected:
	/** Cloth 시뮬레이션 결과를 렌더링 메쉬에 반영 */
	void UpdateRenderMesh();

	/** Cloth 파라미터를 NvCloth 인스턴스에 동기화 */
	void SyncClothParameters();

	/** Rigid body와의 충돌 처리 업데이트 */
	void UpdateCollisions();

private:
	/** Cloth 애셋 참조 */
	UClothAsset* ClothAsset = nullptr;

	/** NvCloth 인스턴스 */
	nv::cloth::Cloth* ClothInstance = nullptr;

	/** 렌더링 메쉬 컴포넌트 (동적 메쉬 업데이트용) */
	UMeshComponent* RenderMeshComponent = nullptr;

	/** Dynamic mesh for cloth rendering */
	UDynamicMesh* DynamicMesh = nullptr;

	/** Material for cloth */
	UMaterial* ClothMaterial = nullptr;

	/** Cloth 시뮬레이션 런타임 상태 */
	bool bSimulatingCloth = false;

	/** 물리 상태가 생성되었는지 */
	bool bClothPhysicsStateCreated = false;
};
