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

	virtual bool HasMesh() const { return false; }
	virtual void SetupEmitterInstance(FParticleEmitterInstance* Inst) {}

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// TypeData는 가장 먼저 표시 (우선순위 0)
	virtual int32 GetDisplayPriority() const override { return 0; }
};
