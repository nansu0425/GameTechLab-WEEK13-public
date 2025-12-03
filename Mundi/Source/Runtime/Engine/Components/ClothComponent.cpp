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

	// Cloth 시뮬레이션이 활성화되어 있으면
	if (bSimulatingCloth && ClothInstance)
	{
		// 시뮬레이션 결과를 렌더링 메쉬에 반영
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

	// ClothCore 초기화 확인
	if (!FClothCore::GetInstance().IsInitialized())
	{
		UE_LOG("[ClothComponent] ERROR: ClothCore not initialized.\n");
		return;
	}

	// ClothAsset 확인
	if (!ClothAsset)
	{
		UE_LOG("[ClothComponent] ERROR: No ClothAsset assigned.\n");
		return;
	}

	// Fabric 생성 (또는 캐시된 것 가져오기)
	nv::cloth::Fabric* Fabric = ClothAsset->GetOrCreateFabric(FClothCore::GetInstance().GetFactory());
	if (!Fabric)
	{
		UE_LOG("[ClothComponent] ERROR: Failed to get/create fabric.\n");
		return;
	}

	// Particle 위치 복사 (NvCloth는 physx::PxVec4 배열 사용)
	const TArray<FVector4>& AssetPositions = ClothAsset->GetParticlePositions();
	TArray<physx::PxVec4> ParticlesCopy;
	ParticlesCopy.reserve(AssetPositions.size());

	for (const FVector4& Pos : AssetPositions)
	{
		ParticlesCopy.emplace_back(Pos.X, Pos.Y, Pos.Z, Pos.W);
	}

	// Cloth 인스턴스 생성
	nv::cloth::Factory* Factory = FClothCore::GetInstance().GetFactory();
	ClothInstance = Factory->createCloth(
		nv::cloth::Range<physx::PxVec4>(ParticlesCopy.data(), ParticlesCopy.data() + ParticlesCopy.size()),
		*Fabric
	);

	if (!ClothInstance)
	{
		UE_LOG("[ClothComponent] ERROR: Failed to create cloth instance.\n");
		return;
	}

	// Cloth 파라미터 설정
	SyncClothParameters();

	// Solver에 추가
	nv::cloth::Solver* Solver = FClothCore::GetInstance().GetSolver();
	if (Solver)
	{
		Solver->addCloth(ClothInstance);
	}

	// Initialize DynamicMesh for rendering
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
	bSimulatingCloth = true;

	// Update mesh with initial particle positions (for editor visualization)
	if (DynamicMesh)
	{
		UpdateRenderMesh();
		UE_LOG("[ClothComponent] Initial mesh rendered for editor\n");
	}

	UE_LOG("[ClothComponent] Cloth physics state created. Particles: %d\n", AssetPositions.size());
}

void UClothComponent::DestroyClothPhysicsState()
{
	UE_LOG("[ClothComponent] DestroyClothPhysicsState called (bClothPhysicsStateCreated=%s, ClothInstance=%p)\n",
		bClothPhysicsStateCreated ? "true" : "false", ClothInstance);

	if (!bClothPhysicsStateCreated)
		return;

	// Solver에서 제거
	if (ClothInstance)
	{
		nv::cloth::Solver* Solver = FClothCore::GetInstance().GetSolver();
		if (Solver)
		{
			Solver->removeCloth(ClothInstance);
		}

		// Cloth 인스턴스 해제
		delete ClothInstance;
		ClothInstance = nullptr;
	}

	// DynamicMesh 해제
	if (DynamicMesh)
	{
		delete DynamicMesh;
		DynamicMesh = nullptr;
	}

	bClothPhysicsStateCreated = false;
	bSimulatingCloth = false;

	UE_LOG("[ClothComponent] Cloth physics state destroyed.\n");
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
	// Safety check: verify cloth physics state is valid
	if (!bClothPhysicsStateCreated || !bSimulatingCloth)
	{
		return;
	}

	if (!ClothInstance || !DynamicMesh || !ClothAsset)
	{
		return;
	}

	// Get current particle positions from NvCloth
	nv::cloth::MappedRange<physx::PxVec4> Particles = ClothInstance->getCurrentParticles();
	if (Particles.size() == 0)
		return;

	static bool bFirstUpdate = true;
	if (bFirstUpdate)
	{
		bFirstUpdate = false;
		UE_LOG("[ClothComponent] First UpdateRenderMesh: %d particles\n", Particles.size());
		UE_LOG("[ClothComponent] First particle position: (%.2f, %.2f, %.2f)\n",
			Particles[0].x, Particles[0].y, Particles[0].z);
	}

	// Build FMeshData from particles
	FMeshData MeshData;
	MeshData.Vertices.reserve(Particles.size());
	MeshData.Color.reserve(Particles.size());
	MeshData.UV.reserve(Particles.size());
	MeshData.Normal.reserve(Particles.size());

	// Add vertex data
	for (size_t i = 0; i < Particles.size(); ++i)
	{
		MeshData.Vertices.emplace_back(Particles[i].x, Particles[i].y, Particles[i].z);
		MeshData.Normal.emplace_back(0.0f, 0.0f, 1.0f); // TODO: Calculate normals
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

void UClothComponent::SyncClothParameters()
{
	if (!ClothInstance)
		return;

	// 중력 설정
	ClothInstance->setGravity(physx::PxVec3(Gravity.X, Gravity.Y, Gravity.Z));
	UE_LOG("[ClothComponent] Gravity set to: (%.1f, %.1f, %.1f)\n", Gravity.X, Gravity.Y, Gravity.Z);

	// 바람 설정 (외부 가속도)
	if (bEnableWind)
	{
		// 중력 + 바람 (합산)
		physx::PxVec3 TotalAcceleration(
			Gravity.X + Wind.X,
			Gravity.Y + Wind.Y,
			Gravity.Z + Wind.Z
		);
		ClothInstance->setGravity(TotalAcceleration);

		UE_LOG("[ClothComponent] Wind enabled: (%.1f, %.1f, %.1f), Total force: (%.1f, %.1f, %.1f)\n",
			Wind.X, Wind.Y, Wind.Z,
			TotalAcceleration.x, TotalAcceleration.y, TotalAcceleration.z);
	}

	// Damping 설정 (속도 감쇠 - 높을수록 빨리 멈춤)
	ClothInstance->setDamping(physx::PxVec3(Damping, Damping, Damping));

	// Drag 설정 (공기 저항 - 높을수록 바람의 영향 감소)
	ClothInstance->setLinearDrag(physx::PxVec3(LinearDrag, LinearDrag, LinearDrag));
	ClothInstance->setAngularDrag(physx::PxVec3(AngularDrag, AngularDrag, AngularDrag));

	// Stiffness frequency 설정 (강성 - 높을수록 원래 모양 유지)
	ClothInstance->setStiffnessFrequency(StiffnessFrequency);

	// Solver iteration 증가 (높을수록 정확하지만 비용 증가)
	ClothInstance->setSolverFrequency(240.0f);

	// Tether constraint 설정 (고정점으로부터 최대 거리 제한)
	ClothInstance->setTetherConstraintScale(1.0f);
	ClothInstance->setTetherConstraintStiffness(1.0f);

	// Self collision 설정 (cloth가 자기 자신과 충돌 방지)
	if (bEnableSelfCollision)
	{
		ClothInstance->setSelfCollisionDistance(SelfCollisionDistance);
		ClothInstance->setSelfCollisionStiffness(0.8f); // 높은 강성으로 관통 방지
		UE_LOG("[ClothComponent] Self collision enabled: distance=%.2f, stiffness=0.8\n", SelfCollisionDistance);
	}
	else
	{
		ClothInstance->setSelfCollisionDistance(0.0f);
	}

	// Friction 설정 (마찰력 추가로 과도한 움직임 방지)
	ClothInstance->setFriction(0.5f);

	UE_LOG("[ClothComponent] Cloth parameters synced.\n");
}
