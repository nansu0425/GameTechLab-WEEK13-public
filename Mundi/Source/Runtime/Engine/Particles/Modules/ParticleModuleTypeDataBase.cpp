#include "pch.h"
#include "ParticleModuleTypeDataBase.h"

void UParticleModuleTypeDataBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
