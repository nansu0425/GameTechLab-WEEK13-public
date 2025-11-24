#include "pch.h"
#include "ParticleSystem.h"
#include "ObjectFactory.h"

UParticleEmitter* UParticleSystem::GetEmitter(int32 EmitterIndex) const
{
	if (EmitterIndex >= 0 && EmitterIndex < Emitters.size())
	{
		return Emitters[EmitterIndex];
	}
	return nullptr;
}

void UParticleSystem::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UObject::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨

	if (!bInIsLoading)
	{
		// 로딩 후, 모든 이미터 정보 캐싱
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (Emitter)
			{
				Emitter->CacheEmitterModuleInfo();
			}
		}
	}
}

void UParticleSystem::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// 모든 이미터 복제
	TArray<UParticleEmitter*> NewEmitters;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			UParticleEmitter* NewEmitter = ObjectFactory::DuplicateObject<UParticleEmitter>(Emitter);
			if (NewEmitter)
			{
				NewEmitter->DuplicateSubObjects(); // 재귀적으로 서브 오브젝트 복제
				NewEmitters.Add(NewEmitter);
			}
		}
	}
	Emitters = NewEmitters;
}
