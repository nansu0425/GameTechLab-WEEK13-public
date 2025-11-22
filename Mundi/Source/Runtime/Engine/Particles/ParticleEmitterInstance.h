#pragma once

#include "ParticleDefinitions.h"
#include "ParticleHelper.h"
#include "ParticleEmitter.h"

class UParticleSystemComponent;

struct FParticleEmitterInstance
{
	// 템플릿 이미터
	UParticleEmitter* SpriteTemplate;

	// 소유 컴포넌트
	UParticleSystemComponent* Component;

	// 현재 LOD 레벨
	int32 CurrentLODLevelIndex;
	UParticleLODLevel* CurrentLODLevel;

	// 파티클 데이터
	/** 파티클 데이터 배열에 대한 포인터 */
	uint8* ParticleData;
	/** 파티클 인덱스 배열에 대한 포인터 */
	uint16* ParticleIndices;
	/** 인스턴스 데이터 배열에 대한 포인터 */
	uint8* InstanceData;
	/** 인스턴스 데이터 배열의 크기 */
	int32 InstancePayloadSize;
	/** 파티클 데이터의 오프셋 */
	int32 PayloadOffset;
	/** 파티클의 총 크기 (in bytes) */
	int32 ParticleSize;
	/** ParticleData 배열안의 파티클들 간의 stride */
	int32 ParticleStride;
	/** 현재 이미터에서 활성화된 파티클들의 수 */
	int32 ActiveParticles;
	/** Monotonically increasing counter. */
	uint32 ParticleCounter;
	/** 파티클 데이터배열에 저장할 수 있는 최대 파티클 활성 수 */
	int32 MaxActiveParticles;

	// 스폰 분수 (부드러운 스폰을 위함)
	float SpawnFraction;

	// Burst 상태 (언리얼 엔진 호환)
	bool bBurstFired;  // Burst가 이미 발생했는지 여부

	// 생성자 / 소멸자
	FParticleEmitterInstance();
	virtual ~FParticleEmitterInstance();

	// 이미터 인스턴스 초기화
	void Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate);

	// 이미터 인스턴스 업데이트
	void Tick(float DeltaTime, bool bSuppressSpawning);

	// 파티클 생성
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);

	// 인덱스의 파티클 제거
	void KillParticle(int32 Index);

	// 모든 파티클 제거
	void KillAllParticles();

	// 파티클 데이터 크기 조정
	void Resize(int32 NewMaxActiveParticles);

	// 파티클 사전 생성
	void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);

	// 파티클 사후 생성
	void PostSpawn(FBaseParticle* Particle, float InterpolationParameter, float SpawnTime);

	// 파티클 업데이트
	void UpdateParticles(float DeltaTime);

	// 인덱스의 파티클 가져오기
	FBaseParticle* GetParticleAtIndex(int32 Index);

	// 렌더링을 위한 동적 데이터 생성
	FDynamicEmitterDataBase* GetDynamicData(bool bSelected);
};
