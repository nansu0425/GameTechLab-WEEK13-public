#pragma once
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>

namespace physx
{
	class PxFoundation;
}

/**
 * @brief NvCloth global manager (Singleton)
 * Manages global Factory and Solver shared by all ClothComponents.
 */
class FClothCore
{
public:
	// Singleton access
	static FClothCore& GetInstance();

	// Initialization
	void Init();
	void Shutdown();

	// NvCloth object accessors
	nv::cloth::Factory* GetFactory() const { return ClothFactory; }
	nv::cloth::Solver* GetSolver() const { return ClothSolver; }

	bool IsInitialized() const { return bInitialized; }

private:
	FClothCore() = default;
	~FClothCore();

	// Non-copyable
	FClothCore(const FClothCore&) = delete;
	FClothCore& operator=(const FClothCore&) = delete;

	// NvCloth global objects
	nv::cloth::Factory* ClothFactory = nullptr;
	nv::cloth::Solver* ClothSolver = nullptr;

	bool bInitialized = false;
};
