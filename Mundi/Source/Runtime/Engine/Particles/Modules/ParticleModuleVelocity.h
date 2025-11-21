#pragma once

#include "ParticleModule.h"
#include "UParticleModuleVelocity.generated.h"

UCLASS(DisplayName="속도 모듈", Description="파티클의 초기 속도와 방향을 설정하는 모듈입니다")
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Velocity")
	FVector StartVelocity = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category="Velocity")
	FVector StartVelocityRange = FVector(0.0f, 0.0f, 0.0f);

	UParticleModuleVelocity() = default;
	virtual ~UParticleModuleVelocity() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
