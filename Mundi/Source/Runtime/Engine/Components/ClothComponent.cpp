#include "pch.h"
#include "ClothComponent.h"
#include "ClothAsset.h"
#include "ClothCore.h"
#include "PhysScene.h"
#include "World.h"
#include <NvCloth/Cloth.h>
#include <NvCloth/Solver.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Range.h>

UClothComponent::UClothComponent()
{
	// Cloth는 물리 시뮬레이션이 필요하지만 BodyInstance는 사용하지 않음
	bSimulatePhysics = false;
}

UClothComponent::~UClothComponent()
{
	DestroyClothPhysicsState();
}

void UClothComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);
}

void UClothComponent::OnUnregister()
{
	DestroyClothPhysicsState();
	Super::OnUnregister();
}

void UClothComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG("[ClothComponent] BeginPlay called\n");
	UE_LOG("[ClothComponent] - bEnableClothSimulation: %s\n", bEnableClothSimulation ? "true" : "false");
	UE_LOG("[ClothComponent] - ClothAsset: %s\n", ClothAsset ? "valid" : "null");

	// Cloth 물리 상태 생성
	if (bEnableClothSimulation && ClothAsset)
	{
		CreateClothPhysicsState();
	}
	else
	{
		UE_LOG("[ClothComponent] Skipping CreateClothPhysicsState (simulation disabled or no asset)\n");
	}
}

void UClothComponent::EndPlay()
{
	DestroyClothPhysicsState();
	Super::EndPlay();
}

void UClothComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	// Debug: Print once to confirm ticking
	static bool bFirstTick = true;
	if (bFirstTick)
	{
		bFirstTick = false;
		UE_LOG("[ClothComponent] TickComponent called (bSimulatingCloth: %s, ClothInstance: %s)\n",
			bSimulatingCloth ? "true" : "false",
			ClothInstance ? "valid" : "null");
	}

	// Cloth 시뮬레이션이 활성화되어 있으면 결과를 렌더링 메쉬에 반영
	if (bSimulatingCloth && ClothInstance)
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

	bClothPhysicsStateCreated = true;
	bSimulatingCloth = true;

	UE_LOG("[ClothComponent] Cloth physics state created. Particles: %d\n", AssetPositions.size());
}

void UClothComponent::DestroyClothPhysicsState()
{
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

void UClothComponent::UpdateRenderMesh()
{
	if (!ClothInstance)
		return;

	// NvCloth로부터 현재 particle 위치 가져오기
	nv::cloth::MappedRange<physx::PxVec4> Particles = ClothInstance->getCurrentParticles();

	// Debug: Print first few particle positions every 60 frames
	static int FrameCounter = 0;
	if (++FrameCounter >= 60)
	{
		FrameCounter = 0;
		if (Particles.size() > 0)
		{
			UE_LOG("[ClothComponent] Particle[0] pos: (%.1f, %.1f, %.1f)\n",
				Particles[0].x, Particles[0].y, Particles[0].z);
		}
		if (Particles.size() > 1)
		{
			UE_LOG("[ClothComponent] Particle[1] pos: (%.1f, %.1f, %.1f)\n",
				Particles[1].x, Particles[1].y, Particles[1].z);
		}
	}

	// TODO: RenderMeshComponent의 vertex buffer 업데이트
	// 현재는 placeholder - 실제 렌더링 시스템에 맞게 구현 필요
	// 예: RenderMeshComponent->UpdateVertexPositions(Particles);
}

void UClothComponent::SyncClothParameters()
{
	if (!ClothInstance)
		return;

	// 중력 설정
	ClothInstance->setGravity(physx::PxVec3(Gravity.X, Gravity.Y, Gravity.Z));

	// Damping 설정
	ClothInstance->setDamping(physx::PxVec3(Damping, Damping, Damping));

	// Drag 설정
	ClothInstance->setLinearDrag(physx::PxVec3(LinearDrag, LinearDrag, LinearDrag));
	ClothInstance->setAngularDrag(physx::PxVec3(AngularDrag, AngularDrag, AngularDrag));

	// Stiffness frequency 설정
	ClothInstance->setStiffnessFrequency(StiffnessFrequency);

	// Self collision 설정
	if (bEnableSelfCollision)
	{
		ClothInstance->setSelfCollisionDistance(SelfCollisionDistance);
		ClothInstance->setSelfCollisionStiffness(0.5f);
	}
	else
	{
		ClothInstance->setSelfCollisionDistance(0.0f);
	}

	UE_LOG("[ClothComponent] Cloth parameters synced.\n");
}

void UClothComponent::UpdateCollisions()
{
	if (!ClothInstance)
		return;

	// TODO: PhysScene으로부터 주변 rigid body 쿼리하여 collision sphere/capsule 추가
	// 예시:
	// TArray<FBodyInstance*> NearbyBodies = GetWorld()->GetPhysScene()->QueryNearbyBodies(GetComponentLocation(), 500.0f);
	// for (FBodyInstance* Body : NearbyBodies)
	// {
	//     FVector Pos = Body->GetPosition();
	//     float Radius = Body->GetBoundingRadius();
	//     // ClothInstance->setSpheres(...)
	// }

	// 현재는 placeholder - 6단계에서 구현 예정
}
