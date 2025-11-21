#include "pch.h"
#include "ParticleModule.h"

void UParticleModule::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UObject::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨
}
