#include "pch.h"
#include "ParticleEmitter.h"
#include "ObjectFactory.h"

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// 파티클 크기 계산
	ParticleSize = sizeof(FBaseParticle);

	// 모듈별 추가 크기 계산을 여기에 추가 가능
	// 예를 들어, 모듈이 파티클에 페이로드 데이터를 추가할 수 있음

	// 모든 LOD 레벨의 모듈 정보 캐싱
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->CacheModuleInfo();
		}
	}
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
	if (LODIndex >= 0 && LODIndex < LODLevels.size())
	{
		return LODLevels[LODIndex];
	}
	return nullptr;
}

void UParticleEmitter::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UObject::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨

	if (!bInIsLoading)
	{
		// 로딩 후, 이미터 정보 캐싱
		CacheEmitterModuleInfo();
	}
}

void UParticleEmitter::DuplicateSubObjects()
{
	UObject::DuplicateSubObjects();

	// 모든 LOD 레벨 복제
	TArray<UParticleLODLevel*> NewLODLevels;
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			UParticleLODLevel* NewLOD = ObjectFactory::DuplicateObject<UParticleLODLevel>(LODLevel);
			if (NewLOD)
			{
				NewLOD->DuplicateSubObjects(); // 재귀적으로 서브 오브젝트 복제
				NewLODLevels.Add(NewLOD);
			}
		}
	}
	LODLevels = NewLODLevels;

	// 이미터 정보 재캐싱
	CacheEmitterModuleInfo();
}
