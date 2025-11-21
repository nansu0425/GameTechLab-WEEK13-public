#pragma once

#include "ParticleModule.h"
#include "UParticleModuleSize.generated.h"

UCLASS(DisplayName="크기 모듈", Description="파티클의 크기를 설정하는 모듈입니다")
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Size")
	FVector StartSize = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Size")
	FVector StartSizeRange = FVector(0.0f, 0.0f, 0.0f);

	UParticleModuleSize() = default;
	virtual ~UParticleModuleSize() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
