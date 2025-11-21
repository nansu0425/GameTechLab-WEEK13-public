#pragma once

#include "ParticleDefinitions.h"

// 전방 선언
struct FParticleEmitterInstance;

// 파티클 데이터에서 파티클 포인터를 선언하는 헬퍼 매크로
// SpawnParticles 내부에서 사용 (ParticleBase를 Particle로 캐스팅)
#define DECLARE_PARTICLE_PTR \
	FBaseParticle* Particle = (FBaseParticle*)ParticleBase

// 파티클 업데이트 루프를 시작하는 헬퍼 매크로
// 이미터 인스턴스의 모든 활성 파티클을 순회합니다
#define BEGIN_UPDATE_LOOP \
	{ \
		FBaseParticle* Particle = nullptr; \
		for (int32 ParticleIndex = 0; ParticleIndex < Owner->ActiveParticles; ParticleIndex++) \
		{ \
			const int32 CurrentIndex = Owner->ParticleIndices[ParticleIndex]; \
			const uint8* ParticleBase = Owner->ParticleData + (CurrentIndex * Owner->ParticleStride); \
			Particle = (FBaseParticle*)ParticleBase; \
			if (Particle == nullptr) continue;

// 파티클 업데이트 루프를 종료하는 헬퍼 매크로
#define END_UPDATE_LOOP \
		} \
	}

// 파티클 기본 크기를 가져오는 헬퍼 함수
inline FVector GetParticleBaseSize(const FBaseParticle& Particle)
{
	return FVector(1.0f, 1.0f, 1.0f);
}

// 인덱스로 파티클을 가져오는 헬퍼 함수
inline FBaseParticle* GetParticleAtIndex(FParticleEmitterInstance* Instance, int32 Index);
