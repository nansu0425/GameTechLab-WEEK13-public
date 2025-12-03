#include "pch.h"
#include "ClothComponent.h"
#include "ClothAsset.h"
#include "ClothCore.h"
#include "PhysScene.h"
#include "World.h"
#include "DynamicMesh.h"
#include "Material.h"
#include "RHIDevice.h"
#include <NvCloth/Cloth.h>
#include <NvCloth/Solver.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Range.h>
#include "MeshBatchElement.h"
#include "SceneView.h"

UClothComponent::UClothComponent()
{
	// Cloth는 물리 시뮬레이션이 필요하지만 BodyInstance는 사용하지 않음
	bSimulatePhysics = false;

	// Make sure component is visible and editable
	bIsVisible = true;
	bIsEditable = true;

	// Enable ticking to update render mesh every frame
	bCanEverTick = true;
	bTickInEditor = true;
}

UClothComponent::~UClothComponent()
{
	DestroyClothPhysicsState();

	if (DynamicMesh)
	{
		delete DynamicMesh;
		DynamicMesh = nullptr;
	}
}

void UClothComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	UE_LOG("[ClothComponent] OnRegister called (World=%s, bClothPhysicsStateCreated=%s)\n",
		InWorld ? "valid" : "null",
		bClothPhysicsStateCreated ? "true" : "false");

	// ★ 에디터 월드에서도 Cloth 시뮬레이션 활성화
	// ClothAsset이 있고 ClothCore가 초기화되어 있으면 바로 생성
	if (InWorld && !InWorld->bPie && bEnableClothSimulation && ClothAsset)
	{
		if (FClothCore::GetInstance().IsInitialized() && !bClothPhysicsStateCreated)
		{
			UE_LOG("[ClothComponent] Creating cloth physics state for editor\n");
			CreateClothPhysicsState();
		}
	}
}

void UClothComponent::OnUnregister()
{
	DestroyClothPhysicsState();
	Super::OnUnregister();
}

void UClothComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	// Just update render mesh with static positions (no physics simulation)
	if (DynamicMesh && ClothAsset)
	{
		UpdateRenderMesh();
	}
}

void UClothComponent::SetClothAsset(UClothAsset* InAsset)
{
	if (ClothAsset == InAsset)
		return;

	// 기존 물리 상태 파괴
	DestroyClothPhysicsState();

	ClothAsset = InAsset;

	// 새 물리 상태 생성
	if (bEnableClothSimulation && ClothAsset)
	{
		CreateClothPhysicsState();
	}
}

void UClothComponent::CreateClothPhysicsState()
{
	// 이미 생성되어 있으면 무시
	if (bClothPhysicsStateCreated)
		return;

	// ClothAsset 확인
	if (!ClothAsset)
	{
		UE_LOG("[ClothComponent] ERROR: No ClothAsset assigned.\n");
		return;
	}

	const TArray<FVector4>& AssetPositions = ClothAsset->GetParticlePositions();

	// Initialize DynamicMesh for rendering only (no physics)
	if (!DynamicMesh)
	{
		ID3D11Device* D3DDevice = GEngine.GetRHIDevice()->GetDevice();
		if (D3DDevice)
		{
			DynamicMesh = new UDynamicMesh();

			// Max vertices = particle count, max indices = triangle count * 3
			uint32 MaxVertices = AssetPositions.size();
			uint32 MaxIndices = ClothAsset->GetTriangleIndices().size();

			DynamicMesh->Initialize(MaxVertices, MaxIndices, D3DDevice, EVertexLayoutType::PositionColorTexturNormal);

			UE_LOG("[ClothComponent] DynamicMesh initialized: %d vertices, %d indices\n", MaxVertices, MaxIndices);
		}
	}

	bClothPhysicsStateCreated = true;

	// Update mesh with initial particle positions (for editor visualization)
	if (DynamicMesh)
	{
		UpdateRenderMesh();
		UE_LOG("[ClothComponent] Initial mesh rendered for editor\n");
	}

	UE_LOG("[ClothComponent] Cloth render state created. Particles: %d\n", AssetPositions.size());
}

void UClothComponent::DestroyClothPhysicsState()
{
	UE_LOG("[ClothComponent] DestroyClothPhysicsState called (bClothPhysicsStateCreated=%s)\n",
		bClothPhysicsStateCreated ? "true" : "false");

	if (!bClothPhysicsStateCreated)
		return;

	// DynamicMesh 해제
	if (DynamicMesh)
	{
		delete DynamicMesh;
		DynamicMesh = nullptr;
	}

	bClothPhysicsStateCreated = false;

	UE_LOG("[ClothComponent] Cloth render state destroyed.\n");
}

void UClothComponent::SetEnableClothSimulation(bool bEnable)
{
	if (bEnableClothSimulation == bEnable)
		return;

	bEnableClothSimulation = bEnable;

	if (bEnable)
	{
		if (ClothAsset && !bClothPhysicsStateCreated)
		{
			CreateClothPhysicsState();
		}
	}
	else
	{
		DestroyClothPhysicsState();
	}
}

void UClothComponent::SetRenderMeshComponent(UMeshComponent* InMeshComponent)
{
	RenderMeshComponent = InMeshComponent;
}

void UClothComponent::SetClothMaterial(UMaterial* InMaterial)
{
	ClothMaterial = InMaterial;
}

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	static bool bFirstCall = true;
	if (bFirstCall)
	{
		bFirstCall = false;
		UE_LOG("[ClothComponent] CollectMeshBatches called\n");
	}

	if (!DynamicMesh || !DynamicMesh->IsInitialized())
	{
		static bool bLoggedInit = false;
		if (!bLoggedInit)
		{
			bLoggedInit = true;
			UE_LOG("[ClothComponent] DynamicMesh not initialized: DynamicMesh=%p, IsInit=%s\n",
				DynamicMesh, DynamicMesh ? (DynamicMesh->IsInitialized() ? "true" : "false") : "null");
		}
		return;
	}

	if (DynamicMesh->GetCurrentVertexCount() == 0 || DynamicMesh->GetCurrentIndexCount() == 0)
	{
		static bool bLoggedEmpty = false;
		if (!bLoggedEmpty)
		{
			bLoggedEmpty = true;
			UE_LOG("[ClothComponent] Mesh data empty: Vertices=%d, Indices=%d\n",
				DynamicMesh->GetCurrentVertexCount(), DynamicMesh->GetCurrentIndexCount());
		}
		return;
	}

	// Use ClothMaterial if available, otherwise create a default one
	UMaterial* MaterialToUse = ClothMaterial;
	if (!MaterialToUse)
	{
		// Use the engine's default material (UberLit)
		MaterialToUse = UResourceManager::GetInstance().GetDefaultMaterial();
		if (!MaterialToUse)
		{
			static bool bLoggedMaterial = false;
			if (!bLoggedMaterial)
			{
				bLoggedMaterial = true;
				UE_LOG("[ClothComponent] Failed to get default material\n");
			}
			return;
		}
	}

	UShader* ShaderToUse = MaterialToUse->GetShader();
	if (!ShaderToUse)
	{
		static bool bLoggedShader = false;
		if (!bLoggedShader)
		{
			bLoggedShader = true;
			UE_LOG("[ClothComponent] Material has no shader\n");
		}
		return;
	}

	TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
	if (MaterialToUse->GetShaderMacros().Num() > 0)
	{
		ShaderMacros.Append(MaterialToUse->GetShaderMacros());
	}

	FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);
	if (!ShaderVariant)
	{
		static bool bLoggedVariant = false;
		if (!bLoggedVariant)
		{
			bLoggedVariant = true;
			UE_LOG("[ClothComponent] Failed to compile shader variant\n");
		}
		return;
	}

	FMeshBatchElement BatchElement;
	BatchElement.VertexBuffer = DynamicMesh->GetVertexBuffer();
	BatchElement.IndexBuffer = DynamicMesh->GetIndexBuffer();
	BatchElement.IndexCount = DynamicMesh->GetCurrentIndexCount();
	BatchElement.WorldMatrix = GetWorldMatrix();
	BatchElement.Material = MaterialToUse;

	// Shader info
	BatchElement.VertexShader = ShaderVariant->VertexShader;
	BatchElement.PixelShader = ShaderVariant->PixelShader;
	BatchElement.InputLayout = ShaderVariant->InputLayout;
	BatchElement.VertexStride = sizeof(FVertexDynamic);

	static bool bLoggedBatch = false;
	if (!bLoggedBatch)
	{
		bLoggedBatch = true;
		FVector WorldPos = GetWorldLocation();
		UE_LOG("[ClothComponent] Adding mesh batch: Vertices=%d, Indices=%d\n",
			DynamicMesh->GetCurrentVertexCount(), DynamicMesh->GetCurrentIndexCount());
		UE_LOG("[ClothComponent] World Position: (%.1f, %.1f, %.1f)\n",
			WorldPos.X, WorldPos.Y, WorldPos.Z);
		UE_LOG("[ClothComponent] Shader: VS=%p, PS=%p, IL=%p, Stride=%d\n",
			BatchElement.VertexShader, BatchElement.PixelShader, BatchElement.InputLayout, BatchElement.VertexStride);
	}

	OutMeshBatchElements.push_back(BatchElement);
}

void UClothComponent::UpdateRenderMesh()
{
	if (!DynamicMesh || !ClothAsset)
	{
		return;
	}

	// Use static particle positions from ClothAsset (no physics simulation)
	const TArray<FVector4>& Particles = ClothAsset->GetParticlePositions();
	if (Particles.empty())
		return;

	static bool bFirstUpdate = true;
	if (bFirstUpdate)
	{
		bFirstUpdate = false;
		UE_LOG("[ClothComponent] First UpdateRenderMesh: %d particles\n", Particles.size());
		UE_LOG("[ClothComponent] First particle position: (%.2f, %.2f, %.2f)\n",
			Particles[0].X, Particles[0].Y, Particles[0].Z);
	}

	// Build FMeshData from static particles
	FMeshData MeshData;
	MeshData.Vertices.reserve(Particles.size());
	MeshData.Color.reserve(Particles.size());
	MeshData.UV.reserve(Particles.size());
	MeshData.Normal.reserve(Particles.size());

	// Add vertex data
	for (size_t i = 0; i < Particles.size(); ++i)
	{
		MeshData.Vertices.emplace_back(Particles[i].X, Particles[i].Y, Particles[i].Z);
		MeshData.Normal.emplace_back(0.0f, 0.0f, 1.0f); // Simple normal
		MeshData.UV.emplace_back(0.0f, 0.0f); // TODO: Calculate UVs
		MeshData.Color.emplace_back(0.8f, 0.8f, 0.8f, 1.0f);
	}

	// Add indices
	const TArray<uint32>& Indices = ClothAsset->GetTriangleIndices();
	MeshData.Indices = Indices;

	// Update DynamicMesh
	ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();
	if (Context)
	{
		DynamicMesh->UpdateData(&MeshData, Context);

		static bool bLoggedUpdate = false;
		if (!bLoggedUpdate)
		{
			bLoggedUpdate = true;
			UE_LOG("[ClothComponent] DynamicMesh updated: %d vertices, %d indices\n",
				MeshData.Vertices.size(), MeshData.Indices.size());
		}
	}
}

