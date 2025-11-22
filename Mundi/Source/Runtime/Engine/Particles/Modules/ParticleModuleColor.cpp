#include "pch.h"
#include "ParticleModuleColor.h"
#include "ParticleEmitterInstance.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 초기 색상 설정
	ParticleBase->Color = StartColor;
}

void UParticleModuleColor::Update(FModuleUpdateContext& Context)
{
	// 언리얼 엔진 방식: 모든 파티클 업데이트 (Context 사용)
	BEGIN_UPDATE_LOOP;
		float Alpha = Particle.RelativeTime;
		Particle.Color = FLinearColor::Lerp(StartColor, EndColor, Alpha);
	END_UPDATE_LOOP;
}

void UParticleModuleColor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
