#pragma once

#include "ParticleModule.h"
#include "UParticleModuleRotation.generated.h"

UCLASS(DisplayName="초기 회전", Description="파티클의 초기 회전값을 설정하는 모듈입니다")
class UParticleModuleRotation : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Rotation")
	float StartRotation = 0.0f;

	UParticleModuleRotation() = default;
	virtual ~UParticleModuleRotation() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
