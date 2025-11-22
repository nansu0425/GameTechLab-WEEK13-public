#include "pch.h"
#include "ParticleModuleLifetime.h"
#include <random>

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 최소값과 최대값 사이의 랜덤 수명 계산
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(MinLifetime, MaxLifetime);

	// 언리얼 엔진 방식: Lifetime 대신 OneOverMaxLifetime 사용 (나눗셈 최적화)
	float Lifetime = dist(gen);
	ParticleBase->OneOverMaxLifetime = (Lifetime > 0.0f) ? (1.0f / Lifetime) : 0.0f;
	ParticleBase->RelativeTime = 0.0f;

	UE_LOG("[Lifetime] Spawn: Lifetime=%.2f, OneOverMaxLifetime=%.4f", Lifetime, ParticleBase->OneOverMaxLifetime);
}

void UParticleModuleLifetime::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
