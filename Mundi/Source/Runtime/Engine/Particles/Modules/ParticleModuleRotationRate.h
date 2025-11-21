#pragma once

#include "ParticleModule.h"
#include "UParticleModuleRotationRate.generated.h"

UCLASS(DisplayName="회전 속도", Description="파티클의 회전 속도를 설정하는 모듈입니다")
class UParticleModuleRotationRate : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Rotation")
	float StartRotationRate = 0.0f;

	UParticleModuleRotationRate() = default;
	virtual ~UParticleModuleRotationRate() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
