#include "pch.h"
#include "ParticleModuleLocation.h"
#include <random>

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 랜덤 위치 오프셋
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	FVector RandomOffset(
		dist(gen) * StartLocationRange.X,
		dist(gen) * StartLocationRange.Y,
		dist(gen) * StartLocationRange.Z
	);

	ParticleBase->Location = StartLocation + RandomOffset;
}

void UParticleModuleLocation::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
