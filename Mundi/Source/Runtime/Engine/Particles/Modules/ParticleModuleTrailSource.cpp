#include "pch.h"
#include "ParticleModuleTrailSource.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSystemComponent.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"

void UParticleModuleTrailSource::Update(FModuleUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;

	if (SourceName.empty())
	{
		return;
	}

	UParticleSystemComponent* Component = Owner.Component;
	if (!Component || !Component->Template)
	{
		return;
	}

	FParticleEmitterInstance* SourceEmitter = nullptr;
	for (FParticleEmitterInstance* EmitterInstance : Component->EmitterInstances)
	{
		if (EmitterInstance && EmitterInstance->CurrentLODLevel &&
		    EmitterInstance->CurrentLODLevel->RequiredModule)
		{
			FString EmitterName = EmitterInstance->CurrentLODLevel->RequiredModule->EmitterName;
			if (EmitterName == SourceName)
			{
				SourceEmitter = EmitterInstance;
				break;
			}
		}
	}

	if (!SourceEmitter || SourceEmitter->ActiveParticles == 0)
	{
		return;
	}

	// 이번 프레임에 활성 상태인 파티클 포인터 수집
	TArray<FBaseParticle*> ActiveSourceParticles;
	ActiveSourceParticles.Reserve(SourceEmitter->ActiveParticles);

	for (int32 i = 0; i < SourceEmitter->ActiveParticles; i++)
	{
		FBaseParticle* SourceParticle = SourceEmitter->GetParticleAtIndex(i);
		if (!SourceParticle)
		{
			continue;
		}

		ActiveSourceParticles.Add(SourceParticle);

		FVector CurrentPos = SourceParticle->Location;

		// 마지막 생성 위치 확인
		FVector* LastPos = LastSpawnPositions.Find(SourceParticle);

		bool bShouldSpawn = false;

		if (LastPos)
		{
			// 거리 체크
			float Distance = FVector::Distance(CurrentPos, *LastPos);
			if (Distance >= MinSpawnDistance)
			{
				bShouldSpawn = true;
			}
		}
		else
		{
			// 처음 추적하는 파티클이면 바로 생성
			bShouldSpawn = true;
		}

		// Trail 파티클 생성
		if (bShouldSpawn)
		{
			// 속도 계산
			FVector SpawnVelocity(0.0f, 0.0f, 0.0f);
			if (bInheritSourceVelocity)
			{
				SpawnVelocity = SourceParticle->Velocity * VelocityInheritScale;
			}

			// 파티클 생성 (1개)
			Owner.SpawnParticles(1, 0.0f, 0.0f, CurrentPos, SpawnVelocity);

			// 마지막 생성 위치 업데이트 (operator[] 사용 = 덮어쓰기)
			LastSpawnPositions[SourceParticle] = CurrentPos;
		}
	}

	// 죽은 파티클의 LastSpawnPositions 정리
	TArray<FBaseParticle*> KeysToRemove;
	for (auto& Pair : LastSpawnPositions)
	{
		if (!ActiveSourceParticles.Contains(Pair.first))
		{
			KeysToRemove.Add(Pair.first);
		}
	}

	for (FBaseParticle* Key : KeysToRemove)
	{
		LastSpawnPositions.Remove(Key);
	}
}

void UParticleModuleTrailSource::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadString(InOutHandle, "SourceName", SourceName);
		FJsonSerializer::ReadFloat(InOutHandle, "MinSpawnDistance", MinSpawnDistance);
		FJsonSerializer::ReadBool(InOutHandle, "bInheritSourceVelocity", bInheritSourceVelocity);
		FJsonSerializer::ReadFloat(InOutHandle, "VelocityInheritScale", VelocityInheritScale);
	}
	else
	{
		InOutHandle["SourceName"] = SourceName;
		InOutHandle["MinSpawnDistance"] = MinSpawnDistance;
		InOutHandle["bInheritSourceVelocity"] = bInheritSourceVelocity;
		InOutHandle["VelocityInheritScale"] = VelocityInheritScale;
	}
}
