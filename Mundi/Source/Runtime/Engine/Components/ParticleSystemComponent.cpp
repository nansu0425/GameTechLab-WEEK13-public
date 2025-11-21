#include "pch.h"
#include "ParticleSystemComponent.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	bAutoActivate = true;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearEmitterInstances();

	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		if (RenderData)
		{
			delete RenderData;
		}
	}
	EmitterRenderData.clear();
}

void UParticleSystemComponent::BeginPlay()
{
	USceneComponent::BeginPlay();

	if (bAutoActivate)
	{
		ActivateSystem();
	}
}

void UParticleSystemComponent::EndPlay()
{
	DeactivateSystem();
	USceneComponent::EndPlay();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime);

	// 모든 이미터 인스턴스 틱
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->Tick(DeltaTime, false);
		}
	}

	// 렌더 데이터 업데이트
	UpdateRenderData();
}

void UParticleSystemComponent::ActivateSystem()
{
	if (Template)
	{
		InitializeEmitterInstances();
	}
}

void UParticleSystemComponent::DeactivateSystem()
{
	ClearEmitterInstances();
}

void UParticleSystemComponent::ResetParticles()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->KillAllParticles();
		}
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template != NewTemplate)
	{
		Template = NewTemplate;

		// 활성 상태면 재초기화
		if (EmitterInstances.size() > 0)
		{
			ClearEmitterInstances();
			InitializeEmitterInstances();
		}
	}
}

void UParticleSystemComponent::InitializeEmitterInstances()
{
	ClearEmitterInstances();

	if (!Template)
	{
		return;
	}

	// 이미터 인스턴스 생성
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (Emitter)
		{
			FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
			Instance->Init(this, Emitter);
			EmitterInstances.Add(Instance);
		}
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			delete Instance;
		}
	}
	EmitterInstances.clear();
}

void UParticleSystemComponent::UpdateRenderData()
{
	// 기존 렌더 데이터 제거
	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		if (RenderData)
		{
			delete RenderData;
		}
	}
	EmitterRenderData.clear();

	// 각 이미터 인스턴스에 대한 새 렌더 데이터 생성
	for (int32 i = 0; i < EmitterInstances.size(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];
		if (!Instance || !Instance->CurrentLODLevel)
		{
			continue;
		}

		// TypeDataModule에서 이미터 타입 결정
		bool bIsMeshEmitter = (Instance->CurrentLODLevel->TypeDataModule != nullptr);

		if (bIsMeshEmitter)
		{
			// 메시 이미터 데이터 생성
			FDynamicMeshEmitterData* MeshData = new FDynamicMeshEmitterData();
			MeshData->EmitterIndex = i;
			// TODO: 메시 렌더 데이터 채우기
			EmitterRenderData.Add(MeshData);
		}
		else
		{
			// 스프라이트 이미터 데이터 생성
			FDynamicSpriteEmitterData* SpriteData = new FDynamicSpriteEmitterData();
			SpriteData->EmitterIndex = i;
			// TODO: 스프라이트 렌더 데이터 채우기
			EmitterRenderData.Add(SpriteData);
		}
	}
}

void UParticleSystemComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	USceneComponent::Serialize(bInIsLoading, InOutHandle);

	// UPROPERTY 속성은 리플렉션 시스템에 의해 자동으로 직렬화됨

	if (!bInIsLoading && bAutoActivate)
	{
		// 로딩 후, 자동 활성화가 켜져 있으면 시스템 활성화
		ActivateSystem();
	}
}
