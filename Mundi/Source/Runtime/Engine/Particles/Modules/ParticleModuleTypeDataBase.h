#pragma once

#include "ParticleModule.h"
#include "UParticleModuleTypeDataBase.generated.h"

UCLASS(DisplayName="타입 데이터 베이스", Description="파티클 타입을 정의하는 베이스 모듈입니다")
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleTypeDataBase() = default;
	virtual ~UParticleModuleTypeDataBase() = default;

	virtual void SetupEmitterInstance(FParticleEmitterInstance* Inst) {}

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
