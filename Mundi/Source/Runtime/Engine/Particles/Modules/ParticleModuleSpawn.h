#pragma once

#include "ParticleModule.h"
#include "UParticleModuleSpawn.generated.h"

UCLASS(DisplayName="스폰 모듈", Description="파티클의 생성 빈도와 수량을 제어하는 모듈입니다")
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Spawn")
	float SpawnRate = 10.0f;

	UPROPERTY(EditAnywhere, Category="Spawn")
	int32 BurstCount = 0;

	UParticleModuleSpawn() = default;
	virtual ~UParticleModuleSpawn() = default;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
