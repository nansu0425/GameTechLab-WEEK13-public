#include "pch.h"
#include "ParticleModuleRotationRate.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 초기 회전 속도 설정
	ParticleBase->RotationRate = StartRotationRate;
}

void UParticleModuleRotationRate::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨
}
