#include "pch.h"
#include "ParticleEmitterInstance.h"
#include "Modules/ParticleModule.h"
#include <cstring>

FParticleEmitterInstance::FParticleEmitterInstance()
	: SpriteTemplate(nullptr)
	, Component(nullptr)
	, CurrentLODLevelIndex(0)
	, CurrentLODLevel(nullptr)
	, ParticleData(nullptr)
	, ParticleIndices(nullptr)
	, InstanceData(nullptr)
	, InstancePayloadSize(0)
	, PayloadOffset(0)
	, ParticleSize(0)
	, ParticleStride(0)
	, ActiveParticles(0)
	, ParticleCounter(0)
	, MaxActiveParticles(0)
	, SpawnFraction(0.0f)
{
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	if (ParticleData)
	{
		delete[] ParticleData;
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}

	if (InstanceData)
	{
		delete[] InstanceData;
		InstanceData = nullptr;
	}
}

void FParticleEmitterInstance::Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	Component = InComponent;
	SpriteTemplate = InTemplate;

	if (SpriteTemplate)
	{
		// 현재 LOD 레벨 가져오기 (기본값 0)
		CurrentLODLevelIndex = 0;
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);

		if (CurrentLODLevel)
		{
			// 파티클 크기와 스트라이드 캐싱
			ParticleSize = SpriteTemplate->ParticleSize;
			ParticleStride = ParticleSize;

			// 초기 파티클 데이터 할당
			Resize(100); // Default to 100 particles
		}
	}
}

void FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles)
{
	if (NewMaxActiveParticles == MaxActiveParticles)
	{
		return;
	}

	// 이전 데이터 해제
	if (ParticleData)
	{
		delete[] ParticleData;
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}

	MaxActiveParticles = NewMaxActiveParticles;

	if (MaxActiveParticles > 0)
	{
		// 파티클 데이터 할당 + 인덱스
		int32 ParticleDataSize = MaxActiveParticles * ParticleStride;
		int32 IndicesSize = MaxActiveParticles * sizeof(uint16);
		int32 TotalSize = ParticleDataSize + IndicesSize;

		ParticleData = new uint8[TotalSize];
		memset(ParticleData, 0, TotalSize);

		// 파티클 인덱스는 파티클 데이터 뒤에 위치
		ParticleIndices = (uint16*)(ParticleData + ParticleDataSize);

		// 인덱스 초기화
		for (int32 i = 0; i < MaxActiveParticles; i++)
		{
			ParticleIndices[i] = i;
		}
	}

	ActiveParticles = 0;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	// 기존 파티클 업데이트
	UpdateParticles(DeltaTime);

	// 새 파티클 생성 (억제되지 않은 경우)
	if (!bSuppressSpawning)
	{
		// 단순 생성 속도: 초당 10개 파티클
		float SpawnRate = 10.0f;
		float ParticlesToSpawn = SpawnRate * DeltaTime + SpawnFraction;
		int32 Count = static_cast<int32>(ParticlesToSpawn);
		SpawnFraction = ParticlesToSpawn - Count;

		if (Count > 0)
		{
			SpawnParticles(Count, 0.0f, DeltaTime / Count, FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f));
		}
	}
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	for (int32 i = 0; i < Count; i++)
	{
		// 공간이 있는지 확인
		if (ActiveParticles >= MaxActiveParticles)
		{
			// 더 많은 파티클을 수용하도록 크기 조정
			Resize(MaxActiveParticles * 2);
		}

		// 새 파티클의 인덱스 가져오기
		int32 ParticleIndex = ActiveParticles;
		ActiveParticles++;

		// 파티클 포인터 가져오기
		uint8* ParticleBase = ParticleData + (ParticleIndex * ParticleStride);
		DECLARE_PARTICLE_PTR;

		// 생성 전
		PreSpawn(Particle, InitialLocation, InitialVelocity);

		// 생성 모듈 적용
		for (UParticleModule* Module : CurrentLODLevel->SpawnModules)
		{
			if (Module && Module->bEnabled)
			{
				Module->Spawn(this, 0, StartTime + i * Increment, Particle);
			}
		}

		// 생성 후
		float SpawnTime = StartTime + i * Increment;
		PostSpawn(Particle, static_cast<float>(i) / Count, SpawnTime);

		ParticleCounter++;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!Particle)
	{
		return;
	}

	// 기본값으로 파티클 초기화
	Particle->Location = InitialLocation;
	Particle->Velocity = InitialVelocity;
	Particle->BaseVelocity = InitialVelocity;
	Particle->RelativeTime = 0.0f;
	Particle->Lifetime = 1.0f; // Default lifetime
	Particle->Rotation = 0.0f;
	Particle->RotationRate = 0.0f;
	Particle->Size = FVector(1.0f, 1.0f, 1.0f);
	Particle->Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationParameter, float SpawnTime)
{
	// 생성 후 추가 로직을 여기에 추가할 수 있음
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	// 모든 파티클 업데이트
	for (int32 i = ActiveParticles - 1; i >= 0; i--)
	{
		// 파티클 가져오기
		FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		// 수명 업데이트
		Particle->RelativeTime += DeltaTime / Particle->Lifetime;

		// 수명 초과 시 파티클 제거
		if (Particle->RelativeTime >= 1.0f)
		{
			KillParticle(i);
			continue;
		}

		// 위치 업데이트
		Particle->Location += Particle->Velocity * DeltaTime;

		// 회전 업데이트
		Particle->Rotation += Particle->RotationRate * DeltaTime;

		// 업데이트 모듈 적용
		for (UParticleModule* Module : CurrentLODLevel->UpdateModules)
		{
			if (Module && Module->bEnabled)
			{
				Module->Update(this, 0, DeltaTime);
			}
		}
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	// 마지막 활성 파티클과 교체
	if (Index != ActiveParticles - 1)
	{
		uint16 Temp = ParticleIndices[Index];
		ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
		ParticleIndices[ActiveParticles - 1] = Temp;
	}

	ActiveParticles--;
}

void FParticleEmitterInstance::KillAllParticles()
{
	ActiveParticles = 0;
}

FBaseParticle* FParticleEmitterInstance::GetParticleAtIndex(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return nullptr;
	}

	int32 ParticleIndex = ParticleIndices[Index];
	uint8* ParticleBase = ParticleData + (ParticleIndex * ParticleStride);
	return (FBaseParticle*)ParticleBase;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData(bool bSelected)
{
	// 활성 파티클이 없으면 nullptr 반환
	if (ActiveParticles <= 0)
	{
		return nullptr;
	}

	// 스프라이트 이미터 데이터 생성
	FDynamicSpriteEmitterData* NewData = new FDynamicSpriteEmitterData();

	// 소스 데이터 설정
	NewData->Source.ActiveParticleCount = ActiveParticles;
	NewData->Source.ParticleStride = ParticleStride;

	// 파티클 데이터 복사
	NewData->Source.DataContainer.ParticleDataNumBytes = ActiveParticles * ParticleStride;
	NewData->Source.DataContainer.ParticleIndicesNumShorts = ActiveParticles;
	NewData->Source.DataContainer.MemBlockSize = NewData->Source.DataContainer.ParticleDataNumBytes +
		(NewData->Source.DataContainer.ParticleIndicesNumShorts * sizeof(uint16));

	// 메모리 할당 및 복사
	NewData->Source.DataContainer.ParticleData = new uint8[NewData->Source.DataContainer.MemBlockSize];
	memcpy(NewData->Source.DataContainer.ParticleData, ParticleData, NewData->Source.DataContainer.ParticleDataNumBytes);

	// 인덱스 설정
	NewData->Source.DataContainer.ParticleIndices = (uint16*)(NewData->Source.DataContainer.ParticleData +
		NewData->Source.DataContainer.ParticleDataNumBytes);
	memcpy(NewData->Source.DataContainer.ParticleIndices, ParticleIndices,
		NewData->Source.DataContainer.ParticleIndicesNumShorts * sizeof(uint16));

	// Required 모듈과 Material 설정 (렌더링 시 필요)
	// TODO: FParticleRequiredModule 구조체 정의 후 변환 필요
	NewData->Source.RequiredModule = nullptr;
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		NewData->Source.MaterialInterface = CurrentLODLevel->RequiredModule->Material;
	}

	// 컴포넌트 스케일 설정
	if (Component)
	{
		NewData->Source.Scale = FVector(1.0f, 1.0f, 1.0f); // 임시로 기본값 사용
	}

	return NewData;
}
