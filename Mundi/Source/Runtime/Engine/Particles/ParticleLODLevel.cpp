#include "pch.h"
#include "ParticleLODLevel.h"
#include "ObjectFactory.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleTypeDataBase.h"

void UParticleLODLevel::CacheModuleInfo()
{
	SpawnModules.clear();
	UpdateModules.clear();

	for (UParticleModule* Module : Modules)
	{
		if (Module && Module->bEnabled)
		{
			// 현재 모든 모듈은 스폰과 업데이트 모듈 둘 다임
			// 나중에 모듈 타입에 따라 최적화 가능
			SpawnModules.Add(Module);
			UpdateModules.Add(Module);
		}
	}
}

void UParticleLODLevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UObject::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨
	// RequiredModule, Modules, TypeDataModule은 자동으로 직렬화됨

	if (!bInIsLoading)
	{
		// 로딩 후, 모듈 정보 캐싱
		CacheModuleInfo();
	}
}

void UParticleLODLevel::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// RequiredModule 복제
	if (RequiredModule)
	{
		UParticleModuleRequired* NewRequired = ObjectFactory::DuplicateObject<UParticleModuleRequired>(RequiredModule);
		RequiredModule = NewRequired;
	}

	// 모든 모듈 복제
	TArray<UParticleModule*> NewModules;
	for (UParticleModule* Module : Modules)
	{
		if (Module)
		{
			UParticleModule* NewModule = ObjectFactory::DuplicateObject<UParticleModule>(Module);
			NewModules.Add(NewModule);
		}
	}
	Modules = NewModules;

	// TypeDataModule 복제
	if (TypeDataModule)
	{
		UParticleModuleTypeDataBase* NewTypeData = ObjectFactory::DuplicateObject<UParticleModuleTypeDataBase>(TypeDataModule);
		TypeDataModule = NewTypeData;
	}

	// 모듈 정보 재캐싱
	CacheModuleInfo();
}
