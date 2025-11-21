#include "pch.h"
#include "ParticleModuleLifetime.h"
#include <random>

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 최소값과 최대값 사이의 랜덤 수명
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(MinLifetime, MaxLifetime);

	ParticleBase->Lifetime = dist(gen);
	ParticleBase->RelativeTime = 0.0f;
}

void UParticleModuleLifetime::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
