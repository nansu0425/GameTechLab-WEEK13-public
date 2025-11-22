#include "pch.h"
#include "ParticleSystemComponent.h"
#include "SceneView.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "Shader.h"
#include "ResourceManager.h"

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

	// 버퍼 해제 추가
	if (ParticleVertexBuffer)
	{
		ParticleVertexBuffer->Release();
		ParticleVertexBuffer = nullptr;
	}
	if (ParticleIndexBuffer)
	{
		ParticleIndexBuffer->Release();
		ParticleIndexBuffer = nullptr;
	}
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

	// 4. 버퍼 생성/리사이즈
	EnsureBufferSize(RequiredVertexCount, RequiredIndexCount);

	// 5. 버텍스 데이터 채우기
	FillVertexBuffer(View);

	// 6. FMeshBatchElement 생성
	CreateMeshBatch(OutMeshBatchElements, RequiredIndexCount);
}

// 추가할 함수의 구현
// - EnsureBufferSize: 필요한 정점/인덱스 수를 확인하고 내부 할당 상태를 갱신합니다.
// - FillVertexBuffer: 현재 뷰를 기준으로 정점 버퍼를 채우는 자리표시자(간단한 안전 구현).
// - CreateMeshBatch: 계산된 정점/인덱스 카운트로 FMeshBatchElement을 생성하여 출력 배열에 추가합니다.

void UParticleSystemComponent::EnsureBufferSize(uint32 RequiredVertexCount, uint32 RequiredIndexCount)
{
	ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();

	// Vertex Buffer 생성/리사이즈
	if (RequiredVertexCount > AllocatedVertexCount)
	{
		if (ParticleVertexBuffer)
		{
			ParticleVertexBuffer->Release();
			ParticleVertexBuffer = nullptr;
		}

		// 성장 전략: 2배 또는 최소 128개
		uint32 NewCount = FMath::Max(RequiredVertexCount * 2, 128u);

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(FParticleSpriteVertex);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = Device->CreateBuffer(&Desc, nullptr, &ParticleVertexBuffer);
		if (SUCCEEDED(hr))
		{
			AllocatedVertexCount = NewCount;
		}
	}

	// Index Buffer 생성/리사이즈
	if (RequiredIndexCount > AllocatedIndexCount)
	{
		if (ParticleIndexBuffer)
		{
			ParticleIndexBuffer->Release();
			ParticleIndexBuffer = nullptr;
		}

		uint32 NewCount = FMath::Max(RequiredIndexCount * 2, 256u);

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = NewCount * sizeof(uint32);
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = Device->CreateBuffer(&Desc, nullptr, &ParticleIndexBuffer);
		if (SUCCEEDED(hr))
		{
			AllocatedIndexCount = NewCount;
		}
	}
}

void UParticleSystemComponent::FillVertexBuffer(const FSceneView* View)
{
	if (!ParticleVertexBuffer || !ParticleIndexBuffer || !View)
	{
		return;
	}

	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

	// Vertex Buffer Map
	D3D11_MAPPED_SUBRESOURCE MappedVertex;
	HRESULT hr = Context->Map(ParticleVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVertex);
	if (FAILED(hr))
	{
		return;
	}

	// Index Buffer Map
	D3D11_MAPPED_SUBRESOURCE MappedIndex;
	hr = Context->Map(ParticleIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedIndex);
	if (FAILED(hr))
	{
		Context->Unmap(ParticleVertexBuffer, 0);
		return;
	}

	FParticleSpriteVertex* Vertices = static_cast<FParticleSpriteVertex*>(MappedVertex.pData);
	uint32* Indices = static_cast<uint32*>(MappedIndex.pData);

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	// 쿼드 UV 좌표
	const FVector2D QuadUVs[4] = {
		FVector2D(0.0f, 1.0f),  // 왼쪽 하단
		FVector2D(0.0f, 0.0f),  // 왼쪽 상단
		FVector2D(1.0f, 0.0f),  // 오른쪽 상단
		FVector2D(1.0f, 1.0f)   // 오른쪽 하단
	};

	// 모든 이미터의 파티클 처리
	for (int32 EmitterIdx = 0; EmitterIdx < EmitterRenderData.Num(); EmitterIdx++)
	{
		FDynamicEmitterDataBase* EmitterData = EmitterRenderData[EmitterIdx];
		if (!EmitterData)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		const int32 ParticleCount = Source.ActiveParticleCount;
		const int32 ParticleStride = Source.ParticleStride;
		const uint8* ParticleData = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;

		if (!ParticleData || ParticleCount == 0)
		{
			continue;
		}

		// 각 파티클에 대해 쿼드 생성
		for (int32 i = 0; i < ParticleCount; i++)
		{
			// 파티클 인덱스로 실제 파티클 데이터 접근
			const int32 ParticleIndex = ParticleIndices ? ParticleIndices[i] : i;
			const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(
				ParticleData + ParticleIndex * ParticleStride
			);

			// 컴포넌트 월드 트랜스폼 적용
			FVector ParticleWorldPos = GetWorldLocation() + Particle->Location * GetRelativeScale();
			FVector2D ParticleSize = FVector2D(Particle->Size.X, Particle->Size.Y) * GetRelativeScale().X;

			// 4개 버텍스 생성 (빌보드 정렬 및 Z회전은 GPU에서 수행)
			for (int32 v = 0; v < 4; v++)
			{
				FParticleSpriteVertex& Vertex = Vertices[VertexOffset + v];
				Vertex.WorldPosition = ParticleWorldPos;  // 모든 버텍스가 동일한 중심 위치
				Vertex.Rotation = Particle->Rotation;
				Vertex.UV = QuadUVs[v];
				Vertex.Size = ParticleSize;
				Vertex.Color = Particle->Color;
				Vertex.RelativeTime = Particle->RelativeTime;
			}

			TArray<int> Order = { 0, 1, 2, 0, 2, 3 };

			for (int i = 0; i < Order.Num(); ++i)
				Indices[IndexOffset + 0] = VertexOffset + Order[i];

			VertexOffset += 4;
			IndexOffset += 6;
		}
	}

	Context->Unmap(ParticleIndexBuffer, 0);
	Context->Unmap(ParticleVertexBuffer, 0);
}

void UParticleSystemComponent::CreateMeshBatch(TArray<FMeshBatchElement>& OutMeshBatchElements, uint32 IndexCount)
{
	if (!ParticleVertexBuffer || !ParticleIndexBuffer || IndexCount == 0)
	{
		return;
	}

	// 파티클 머티리얼/셰이더 로드
	UMaterialInterface* Material = UResourceManager::GetInstance().Load<UMaterial>("Shaders/Particle/ParticleSprite.hlsl");
	if (!Material || !Material->GetShader())
	{
		return;
	}

	UShader* Shader = Material->GetShader();
	FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(Material->GetShaderMacros());

	// FMeshBatchElement 생성
	FMeshBatchElement BatchElement;

	BatchElement.VertexShader = ShaderVariant->VertexShader;
	BatchElement.PixelShader = ShaderVariant->PixelShader;
	BatchElement.InputLayout = ShaderVariant->InputLayout;
	BatchElement.Material = Material;
	BatchElement.VertexBuffer = ParticleVertexBuffer;
	BatchElement.IndexBuffer = ParticleIndexBuffer;
	BatchElement.VertexStride = sizeof(FParticleSpriteVertex);

	BatchElement.IndexCount = IndexCount;
	BatchElement.StartIndex = 0;
	BatchElement.BaseVertexIndex = 0;

	// 월드 행렬은 항등 행렬 (버텍스가 이미 월드 좌표)
	BatchElement.WorldMatrix = FMatrix::Identity();
	BatchElement.ObjectID = InternalIndex;
	BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	OutMeshBatchElements.Add(BatchElement);
}
