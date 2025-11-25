#include "pch.h"
#include "ParticleLODLevel.h"
#include "ObjectFactory.h"
#include "JsonSerializer.h"
#include "Modules/ParticleModuleRequired.h"
#include "Modules/ParticleModuleTypeDataBase.h"

void UParticleLODLevel::CacheModuleInfo()
{
	// RequiredModule 보장
	if (RequiredModule == nullptr)
	{
		RequiredModule = ObjectFactory::NewObject<UParticleModuleRequired>();
	}

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
	// 기본 타입 UPROPERTY 자동 직렬화 (Level, bEnabled)
	UObject::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// === RequiredModule 로드 ===
		JSON RequiredJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "RequiredModule", RequiredJson))
		{
			RequiredModule = NewObject<UParticleModuleRequired>();
			RequiredModule->Serialize(true, RequiredJson);
		}

		// === SpawnModule 로드 ===
		JSON SpawnJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "SpawnModule", SpawnJson))
		{
			SpawnModule = NewObject<UParticleModuleSpawn>();
			SpawnModule->Serialize(true, SpawnJson);
		}

		// === TypeDataModule 로드 (다형성 처리) ===
		JSON TypeDataJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "TypeDataModule", TypeDataJson))
		{
			FString TypeString;
			FJsonSerializer::ReadString(TypeDataJson, "Type", TypeString);
			UClass* TypeDataClass = UClass::FindClass(TypeString);
			if (TypeDataClass)
			{
				TypeDataModule = Cast<UParticleModuleTypeDataBase>(
					ObjectFactory::NewObject(TypeDataClass));
				if (TypeDataModule)
				{
					TypeDataModule->Serialize(true, TypeDataJson);
				}
			}
		}

		// === Modules 배열 로드 (다형성 처리) ===
		JSON ModulesJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "Modules", ModulesJson))
		{
			Modules.Empty();
			for (size_t i = 0; i < ModulesJson.size(); ++i)
			{
				JSON& ModuleData = ModulesJson.at(i);
				FString TypeString;
				FJsonSerializer::ReadString(ModuleData, "Type", TypeString);

				UClass* ModuleClass = UClass::FindClass(TypeString);
				if (ModuleClass)
				{
					UParticleModule* Module = Cast<UParticleModule>(ObjectFactory::NewObject(ModuleClass));
					if (Module)
					{
						Module->Serialize(true, ModuleData);
						Modules.Add(Module);
					}
				}
			}
		}

		// 로딩 후 캐싱
		CacheModuleInfo();
	}
	else
	{
		// === RequiredModule 저장 ===
		if (RequiredModule)
		{
			JSON RequiredJson = JSON::Make(JSON::Class::Object);
			RequiredModule->Serialize(false, RequiredJson);
			InOutHandle["RequiredModule"] = RequiredJson;
		}

		// === SpawnModule 저장 ===
		if (SpawnModule)
		{
			JSON SpawnJson = JSON::Make(JSON::Class::Object);
			SpawnModule->Serialize(false, SpawnJson);
			InOutHandle["SpawnModule"] = SpawnJson;
		}

		// === TypeDataModule 저장 (타입 정보 포함) ===
		if (TypeDataModule)
		{
			JSON TypeDataJson = JSON::Make(JSON::Class::Object);
			TypeDataJson["Type"] = TypeDataModule->GetClass()->Name;
			TypeDataModule->Serialize(false, TypeDataJson);
			InOutHandle["TypeDataModule"] = TypeDataJson;
		}

		// === Modules 배열 저장 (타입 정보 포함) ===
		JSON ModulesJson = JSON::Make(JSON::Class::Array);
		for (UParticleModule* Module : Modules)
		{
			if (Module)
			{
				JSON ModuleData = JSON::Make(JSON::Class::Object);
				ModuleData["Type"] = Module->GetClass()->Name;
				Module->Serialize(false, ModuleData);
				ModulesJson.append(ModuleData);
			}
		}
		InOutHandle["Modules"] = ModulesJson;
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
