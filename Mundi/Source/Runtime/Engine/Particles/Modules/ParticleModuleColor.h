#pragma once

#include "ParticleModule.h"
#include "UParticleModuleColor.generated.h"

UCLASS(DisplayName="색상 모듈", Description="파티클의 색상을 정의하며, 시간에 따른 색상 변화를 설정할 수 있습니다")
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Color")
	FLinearColor StartColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Color")
	FLinearColor EndColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UParticleModuleColor()
	{
		bSpawnModule = true;   // 스폰 시 초기 색상 설정
		bUpdateModule = true;  // 매 프레임 색상 보간
	}
	virtual ~UParticleModuleColor() = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FModuleUpdateContext& Context) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
