#include "pch.h"
#include "ParticleModuleAcceleration.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleAcceleration::Update(FModuleUpdateContext& Context)
{
	FVector TotalAcceleration = Acceleration;

	// 중력 활성화 시 적용
	if (bApplyGravity)
	{
		const float GravityZ = -980.0f; // cm/s^2
		TotalAcceleration.Z += GravityZ * GravityScale;
	}

	// 언리얼 엔진 방식: 모든 파티클 업데이트 (Context 사용)
	BEGIN_UPDATE_LOOP;
		Particle.Velocity += TotalAcceleration * DeltaTime;
	END_UPDATE_LOOP;
}

void UParticleModuleAcceleration::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
