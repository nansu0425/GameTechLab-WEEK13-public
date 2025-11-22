#include "pch.h"
#include "ParticleLODLevel.h"
#include "ObjectFactory.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleTypeDataBase.h"

void UParticleLODLevel::CacheModuleInfo()
{
	SpawnModules.clear();
	UpdateModules.clear();

	// 언리얼 엔진 방식: 모듈 타입 플래그에 따라 분류
	for (UParticleModule* Module : Modules)
	{
		if (Module && Module->bEnabled)
		{
			// 스폰 모듈인 경우 SpawnModules에 추가
			if (Module->bSpawnModule)
			{
				SpawnModules.Add(Module);
			}

			// 업데이트 모듈인 경우 UpdateModules에 추가
			if (Module->bUpdateModule)
			{
				UpdateModules.Add(Module);
			}
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

	// 언리얼 엔진 호환: SpawnModule 복제
	if (SpawnModule)
	{
		UParticleModuleSpawn* NewSpawn = ObjectFactory::DuplicateObject<UParticleModuleSpawn>(SpawnModule);
		SpawnModule = NewSpawn;
	}

	// 모듈 정보 재캐싱
	CacheModuleInfo();
}
