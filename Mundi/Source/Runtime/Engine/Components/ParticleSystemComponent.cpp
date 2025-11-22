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

void UParticleSystemComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// 1. 유효성 검사
	if (!IsVisible() || EmitterRenderData.Num() == 0)
	{
		return;
	}

	// 2. 전체 파티클 수 계산
	int32 TotalParticles = 0;
	for (FDynamicEmitterDataBase* EmitterData : EmitterRenderData)
	{
		if (EmitterData)
		{
			TotalParticles += EmitterData->GetSource().ActiveParticleCount;
		}
	}

	if (TotalParticles == 0)
	{
		return;
	}

	// 3. 버퍼 크기 계산 (파티클당 4 버텍스, 6 인덱스 - 쿼드)
	uint32 RequiredVertexCount = TotalParticles * 4;
	uint32 RequiredIndexCount = TotalParticles * 6;

	// 4. TODO: 버퍼 생성/리사이즈
	EnsureBufferSize(RequiredVertexCount, RequiredIndexCount);

	// 5. TODO: 버텍스 데이터 채우기
	// FillVertexBuffer(View);

	// 6. TODO: FMeshBatchElement 생성
	// CreateMeshBatch(OutMeshBatchElements);
}

// 추가할 함수의 구현
// - EnsureBufferSize: 필요한 정점/인덱스 수를 확인하고 내부 할당 상태를 갱신합니다.
// - FillVertexBuffer: 현재 뷰를 기준으로 정점 버퍼를 채우는 자리표시자(간단한 안전 구현).
// - CreateMeshBatch: 계산된 정점/인덱스 카운트로 FMeshBatchElement을 생성하여 출력 배열에 추가합니다.

void UParticleSystemComponent::EnsureBufferSize(uint32 RequiredVertexCount, uint32 RequiredIndexCount)
{
	if (RequiredVertexCount > AllocatedVertexCount)
	{
		if (ParticleVertexBuffer)
		{
			ParticleVertexBuffer->Release();
			ParticleVertexBuffer = nullptr;
		}

		uint32 NewCount = FMath::Max(RequiredVertexCount * 2, 128u);
		
		// D3D11 버퍼 생성
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FParticleSpriteVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		
	}
}

void UParticleSystemComponent::FillVertexBuffer(const FSceneView* View)
{

}

void UParticleSystemComponent::CreateMeshBatch(TArray<FMeshBatchElement>& OutMeshBatchElements)
{

}
