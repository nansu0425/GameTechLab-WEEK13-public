#include "pch.h"
#include "ParticleModuleRequired.h"

void UParticleModuleRequired::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
