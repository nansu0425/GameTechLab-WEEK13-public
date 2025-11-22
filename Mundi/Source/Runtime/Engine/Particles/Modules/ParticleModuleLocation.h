#pragma once

#include "ParticleModule.h"
#include "UParticleModuleLocation.generated.h"

UCLASS(DisplayName="위치 모듈", Description="파티클의 초기 위치를 결정하는 모듈입니다")
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Location")
	FVector StartLocation = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category="Location")
	FVector StartLocationRange = FVector(0.0f, 0.0f, 0.0f);

	UParticleModuleLocation()
	{
		bSpawnModule = true;  // 스폰 시 위치 설정
	}
	virtual ~UParticleModuleLocation() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
