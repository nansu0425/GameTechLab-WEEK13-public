#include "pch.h"
#include "ParticleModuleSpawn.h"

// 언리얼 엔진 호환: 스폰 계산 로직 (책임 분리)
int32 UParticleModuleSpawn::CalculateSpawnCount(float DeltaTime, float& InOutSpawnFraction, bool& bInOutBurstFired)
{
	int32 TotalSpawnCount = 0;

	// 1. Burst 처리 (최초 1회만)
	if (!bInOutBurstFired && BurstCount > 0)
	{
		bInOutBurstFired = true;
		TotalSpawnCount += BurstCount;
	}

	// 2. SpawnRate에 따른 정상 스폰 (부드러운 스폰)
	if (SpawnRate > 0.0f)
	{
		// 언리얼 엔진 방식: 분수 누적으로 프레임 독립적 스폰
		float ParticlesToSpawn = SpawnRate * DeltaTime + InOutSpawnFraction;
		int32 Count = static_cast<int32>(ParticlesToSpawn);
		InOutSpawnFraction = ParticlesToSpawn - Count;  // 남은 분수 보존

		TotalSpawnCount += Count;
	}

	return TotalSpawnCount;
}

void UParticleModuleSpawn::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
