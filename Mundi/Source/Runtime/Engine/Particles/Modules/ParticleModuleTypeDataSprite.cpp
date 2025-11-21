#include "pch.h"
#include "ParticleModuleTypeDataSprite.h"

void UParticleModuleTypeDataSprite::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModuleTypeDataBase::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨
}
