#include "pch.h"
#include "ParticleModuleVelocity.h"
#include <random>

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 랜덤 속도 오프셋
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	FVector RandomOffset(
		dist(gen) * StartVelocityRange.X,
		dist(gen) * StartVelocityRange.Y,
		dist(gen) * StartVelocityRange.Z
	);

	ParticleBase->Velocity = StartVelocity + RandomOffset;
	ParticleBase->BaseVelocity = ParticleBase->Velocity;
}

void UParticleModuleVelocity::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
