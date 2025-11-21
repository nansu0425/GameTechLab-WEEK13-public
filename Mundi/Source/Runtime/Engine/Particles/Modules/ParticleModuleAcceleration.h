#pragma once

#include "ParticleModule.h"
#include "UParticleModuleAcceleration.generated.h"

UCLASS(DisplayName="가속도 모듈", Description="파티클에 가속도(중력 포함)를 적용하는 모듈입니다")
class UParticleModuleAcceleration : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Acceleration")
	FVector Acceleration = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category="Acceleration")
	bool bApplyGravity = false;

	UPROPERTY(EditAnywhere, Category="Acceleration")
	float GravityScale = 1.0f;

	UParticleModuleAcceleration() = default;
	virtual ~UParticleModuleAcceleration() = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
