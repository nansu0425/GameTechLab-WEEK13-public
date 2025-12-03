#pragma once

#include "Object.h"
#include "Math.h"
#include "UClothAsset.generated.h"

namespace nv
{
	namespace cloth
	{
		class Fabric;
		class Factory;
	}
}

/**
 * @brief Cloth 시뮬레이션을 위한 메쉬 데이터 애셋
 *
 * UPhysicsAsset과 유사한 역할을 하는 Cloth 전용 애셋.
 * Particle 위치, 인덱스, Fabric(constraint) 정보를 저장.
 */
UCLASS(DisplayName = "Cloth 애셋", Description = "Cloth 시뮬레이션 메쉬 애셋")
class UClothAsset : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

	UClothAsset();
	~UClothAsset() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	/**
	 * @brief Fabric 생성 또는 캐시된 Fabric 반환
	 *
	 * NvCloth는 메쉬의 constraint 정보를 Fabric으로 cooking하여 재사용.
	 * 최초 호출 시 cooking하고 이후엔 캐시된 Fabric 반환.
	 */
	nv::cloth::Fabric* GetOrCreateFabric(nv::cloth::Factory* Factory);

	/**
	 * @brief 메쉬 데이터 설정 (임포트 시 사용)
	 * @param InPositions Particle 위치 배열 (w = inverse mass)
	 * @param InIndices Triangle 인덱스 배열 (3개씩 묶음)
	 */
	void SetMeshData(const TArray<FVector4>& InPositions, const TArray<uint32>& InIndices);

	/** Particle 위치 접근 (w = inverse mass) */
	const TArray<FVector4>& GetParticlePositions() const { return ParticlePositions; }

	/** Triangle 인덱스 접근 */
	const TArray<uint32>& GetTriangleIndices() const { return TriangleIndices; }

	/** Particle 개수 */
	int32 GetParticleCount() const { return ParticlePositions.Num(); }

	/** Triangle 개수 */
	int32 GetTriangleCount() const { return TriangleIndices.Num() / 3; }

	/** Fabric 캐시 무효화 (메쉬 데이터 변경 시 호출) */
	void InvalidateFabric();

	/**
	 * @brief Create a simple plane cloth mesh for testing
	 * @param Width Plane width
	 * @param Height Plane height
	 * @param ResolutionX Number of vertices along X axis
	 * @param ResolutionY Number of vertices along Y axis
	 * @return New ClothAsset with plane mesh data
	 */
	static UClothAsset* CreatePlaneCloth(float Width, float Height, int32 ResolutionX, int32 ResolutionY);

private:
	/** Particle 위치 및 inverse mass (w 컴포넌트) */
	TArray<FVector4> ParticlePositions;

	/** Triangle 인덱스 (3개씩 묶음) */
	TArray<uint32> TriangleIndices;

	/** Cooked Fabric (캐싱용) */
	nv::cloth::Fabric* CachedFabric = nullptr;

	/** Fabric이 유효한지 */
	bool bFabricValid = false;
};
