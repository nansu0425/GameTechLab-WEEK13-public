#include "pch.h"
#include "ParticleModuleColor.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 초기 색상 설정
	ParticleBase->Color = StartColor;
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	if (!Owner)
	{
		return;
	}

	// 수명에 따라 색상 보간
	BEGIN_UPDATE_LOOP;
		float Alpha = Particle->RelativeTime;
		Particle->Color = FLinearColor::Lerp(StartColor, EndColor, Alpha);
	END_UPDATE_LOOP;
}

void UParticleModuleColor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
