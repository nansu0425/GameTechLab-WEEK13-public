#include "pch.h"
#include "ParticleModuleRotation.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 초기 회전값 설정
	ParticleBase->Rotation = StartRotation;
}

void UParticleModuleRotation::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨
}
