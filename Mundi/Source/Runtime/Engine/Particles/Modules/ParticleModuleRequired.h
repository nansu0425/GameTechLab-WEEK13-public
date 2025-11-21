#pragma once

#include "ParticleModule.h"
#include "UParticleModuleRequired.generated.h"

UCLASS(DisplayName="필수 모듈", Description="이미터에 필수적인 설정을 포함하는 모듈입니다")
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Required")
	FString EmitterName = "Emitter";

	UPROPERTY(EditAnywhere, Category="Required")
	class UMaterialInterface* Material = nullptr;

	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired() = default;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
