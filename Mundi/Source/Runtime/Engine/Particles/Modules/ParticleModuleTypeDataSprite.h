#pragma once

#include "ParticleModuleTypeDataBase.h"
#include "UParticleModuleTypeDataSprite.generated.h"

UCLASS(DisplayName="스프라이트 타입 데이터", Description="스프라이트 파티클을 위한 타입 데이터입니다")
class UParticleModuleTypeDataSprite : public UParticleModuleTypeDataBase
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleTypeDataSprite() = default;
	virtual ~UParticleModuleTypeDataSprite() = default;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
