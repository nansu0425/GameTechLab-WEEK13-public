#include "pch.h"
#include "ParticleModuleSize.h"
#include <random>

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 랜덤 크기 오프셋
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	FVector RandomOffset(
		dist(gen) * StartSizeRange.X,
		dist(gen) * StartSizeRange.Y,
		dist(gen) * StartSizeRange.Z
	);

	ParticleBase->Size = StartSize + RandomOffset;
}

void UParticleModuleSize::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
