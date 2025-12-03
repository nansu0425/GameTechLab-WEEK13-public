#include "pch.h"
#include "ClothCore.h"
#include "PhysicsCore.h"
#include <NvCloth/Callbacks.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>

// Allocator and error callback for NvCloth initialization
// Using the same ones as PhysX
namespace
{
	class ClothAllocatorCallback : public physx::PxAllocatorCallback
	{
	public:
		void* allocate(size_t size, const char* typeName, const char* filename, int line) override
		{
			return _aligned_malloc(size, 16);
		}

		void deallocate(void* ptr) override
		{
			_aligned_free(ptr);
		}
	};

	class ClothErrorCallback : public physx::PxErrorCallback
	{
	public:
		void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
		{
			UE_LOG("[NvCloth Error] %s (%s:%d)\n", message, file, line);
		}
	};

	ClothAllocatorCallback g_ClothAllocator;
	ClothErrorCallback g_ClothErrorCallback;
}

FClothCore& FClothCore::GetInstance()
{
	static FClothCore Instance;
	return Instance;
}

FClothCore::~FClothCore()
{
	Shutdown();
}

void FClothCore::Init()
{
	if (bInitialized)
	{
		UE_LOG("[ClothCore] Already initialized.\n");
		return;
	}

	// PhysX Foundation must be initialized first
	if (!FPhysicsCore::Get().IsInitialized())
	{
		UE_LOG("[ClothCore] ERROR: PhysicsCore must be initialized first!\n");
		return;
	}

	// Initialize NvCloth library
	nv::cloth::InitializeNvCloth(&g_ClothAllocator, &g_ClothErrorCallback, nv::cloth::GetNvClothAssertHandler(), nullptr);

	// Create CPU Factory
	ClothFactory = NvClothCreateFactoryCPU();
	if (!ClothFactory)
	{
		UE_LOG("[ClothCore] ERROR: Failed to create NvCloth factory.\n");
		return;
	}

	// Create Global Solver
	ClothSolver = ClothFactory->createSolver();
	if (!ClothSolver)
	{
		UE_LOG("[ClothCore] ERROR: Failed to create NvCloth solver.\n");
		NvClothDestroyFactory(ClothFactory);
		ClothFactory = nullptr;
		return;
	}

	bInitialized = true;
	UE_LOG("[ClothCore] Initialized successfully.\n");
}

void FClothCore::Shutdown()
{
	if (!bInitialized)
		return;

	// Destroy Solver
	if (ClothSolver)
	{
		delete ClothSolver;
		ClothSolver = nullptr;
	}

	// Destroy Factory
	if (ClothFactory)
	{
		NvClothDestroyFactory(ClothFactory);
		ClothFactory = nullptr;
	}

	bInitialized = false;
	UE_LOG("[ClothCore] Shutdown complete.\n");
}
