#include "pch.h"
#include "ParticleModuleSpawn.h"
#include "ParticleEmitterInstance.h"

// FParticleBurst 직렬화
void FParticleBurst::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	if (bIsLoading)
	{
		FJsonSerializer::ReadInt32(InOutHandle, "Count", Count);
		FJsonSerializer::ReadInt32(InOutHandle, "CountLow", CountLow);
		FJsonSerializer::ReadFloat(InOutHandle, "Time", Time);
	}
	else
	{
		InOutHandle["Count"] = Count;
		InOutHandle["CountLow"] = CountLow;
		InOutHandle["Time"] = Time;
	}
}

// 언리얼 엔진 호환: 스폰 계산 로직 (책임 분리)
int32 UParticleModuleSpawn::CalculateSpawnCount(FParticleEmitterInstance* Owner, float DeltaTime, float& InOutSpawnFraction)
{
	int32 TotalSpawnCount = 0;

	if (!Owner)
	{
		return TotalSpawnCount;
	}

	// 1. BurstList 처리 (시간 기반 다중 버스트)
	float BurstScaleValue = BurstScale.GetValue(Owner->EmitterTime, Owner->RandomStream, Owner->Component);

	for (int32 i = 0; i < BurstList.Num(); ++i)
	{
		const FParticleBurst& Burst = BurstList[i];

		// Count가 0 이하면 건너뛰기 (언리얼 방식)
		if (Burst.Count <= 0)
		{
			continue;
		}

		// BurstFired 배열 범위 체크
		if (i >= Owner->BurstFired.Num())
		{
			continue;
		}

		// 언리얼 방식: Burst.Time은 0..1 정규화 (이미터 수명 기준)
		// EmitterDuration이 0(무한)이면 절대 시간(초)으로 처리
		float BurstTimeActual = Burst.Time;
		if (Owner->EmitterDurationActual > 0.0f)
		{
			BurstTimeActual = Burst.Time * Owner->EmitterDurationActual;
		}

		// 이 버스트가 아직 발생하지 않았고, 시간이 되었으면 발생
		if (!Owner->BurstFired[i] && Owner->EmitterTime >= BurstTimeActual)
		{
			Owner->BurstFired[i] = true;

			// 버스트 개수 계산 (CountLow가 -1이 아니면 랜덤 범위)
			int32 BurstCountValue;
			if (Burst.CountLow >= 0 && Burst.CountLow < Burst.Count)
			{
				// GetRangeFloat로 float 범위를 얻고 int로 캐스팅
				BurstCountValue = static_cast<int32>(Owner->RandomStream.GetRangeFloat(
					static_cast<float>(Burst.CountLow),
					static_cast<float>(Burst.Count + 1)  // +1 해서 Count까지 포함
				));
				// 범위 클램핑
				if (BurstCountValue > Burst.Count) BurstCountValue = Burst.Count;
			}
			else
			{
				BurstCountValue = Burst.Count;
			}

			// BurstScale 적용
			TotalSpawnCount += static_cast<int32>(BurstCountValue * BurstScaleValue);
		}
	}

	// 2. SpawnRate에 따른 정상 스폰 (부드러운 스폰)
	// Distribution에서 현재 SpawnRate 값 가져오기 (이미터 시간 기반 - 커브 타입 지원)
	float SpawnRateValue = SpawnRate.GetValue(Owner->EmitterTime, Owner->RandomStream, Owner->Component);
	if (SpawnRateValue > 0.0f)
	{
		// 언리얼 엔진 방식: 분수 누적으로 프레임 독립적 스폰
		float ParticlesToSpawn = SpawnRateValue * DeltaTime + InOutSpawnFraction;
		int32 Count = static_cast<int32>(ParticlesToSpawn);
		InOutSpawnFraction = ParticlesToSpawn - Count;  // 남은 분수 보존

		TotalSpawnCount += Count;
	}

	return TotalSpawnCount;
}

void UParticleModuleSpawn::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	JSON TempJson;
	if (bInIsLoading)
	{
		// SpawnRate
		if (FJsonSerializer::ReadObject(InOutHandle, "SpawnRate", TempJson))
			SpawnRate.Serialize(true, TempJson);

		// BurstList (배열)
		BurstList.Empty();
		if (InOutHandle.hasKey("BurstList") && InOutHandle["BurstList"].JSONType() == JSON::Class::Array)
		{
			JSON& BurstArray = InOutHandle["BurstList"];
			for (auto& BurstJson : BurstArray.ArrayRange())
			{
				FParticleBurst Burst;
				Burst.Serialize(true, BurstJson);
				BurstList.Add(Burst);
			}
		}

		// BurstScale
		if (FJsonSerializer::ReadObject(InOutHandle, "BurstScale", TempJson))
			BurstScale.Serialize(true, TempJson);
	}
	else
	{
		// SpawnRate
		TempJson = JSON::Make(JSON::Class::Object);
		SpawnRate.Serialize(false, TempJson);
		InOutHandle["SpawnRate"] = TempJson;

		// BurstList (배열)
		JSON BurstArray = JSON::Make(JSON::Class::Array);
		for (int32 i = 0; i < BurstList.Num(); ++i)
		{
			JSON BurstJson = JSON::Make(JSON::Class::Object);
			BurstList[i].Serialize(false, BurstJson);
			BurstArray.append(BurstJson);
		}
		InOutHandle["BurstList"] = BurstArray;

		// BurstScale
		TempJson = JSON::Make(JSON::Class::Object);
		BurstScale.Serialize(false, TempJson);
		InOutHandle["BurstScale"] = TempJson;
	}
}
