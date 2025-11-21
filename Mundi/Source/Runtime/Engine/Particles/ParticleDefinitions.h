#pragma once

#include "Vector.h"
#include "Color.h"
#include "UEContainer.h"

class UMaterialInterface;
struct FParticleRequiredModule;

// 동적 이미터 타입
enum class EDynamicEmitterType : uint8
{
	Sprite = 0,
	Mesh = 1,
	Unknown = 255
};

// 기본 파티클 구조체
struct FBaseParticle
{
	FVector    Location;
	FVector    Velocity;
	float      RelativeTime;
	float      Lifetime;
	FVector    BaseVelocity;
	float      Rotation;
	float      RotationRate;
	FVector    Size;
	FLinearColor Color;

	FBaseParticle()
		: Location(FVector(0.0f, 0.0f, 0.0f))
		, Velocity(FVector(0.0f, 0.0f, 0.0f))
		, RelativeTime(0.0f)
		, Lifetime(0.0f)
		, BaseVelocity(FVector(0.0f, 0.0f, 0.0f))
		, Rotation(0.0f)
		, RotationRate(0.0f)
		, Size(FVector(1.0f, 1.0f, 1.0f))
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{
	}
};

// 파티클 데이터 컨테이너
struct FParticleDataContainer
{
	int32 MemBlockSize;
	int32 ParticleDataNumBytes;
	int32 ParticleIndicesNumShorts;
	uint8* ParticleData;        // 할당된 메모리 블록
	uint16* ParticleIndices;    // 메모리 블록 끝에 위치 (별도 할당 안함)

	FParticleDataContainer()
		: MemBlockSize(0)
		, ParticleDataNumBytes(0)
		, ParticleIndicesNumShorts(0)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
	{
	}

	~FParticleDataContainer()
	{
		if (ParticleData)
		{
			delete[] ParticleData;
			ParticleData = nullptr;
			ParticleIndices = nullptr;
		}
	}
};

// 동적 이미터 리플레이 데이터 베이스 (렌더 스레드용)
struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType;
	int32 ActiveParticleCount;
	int32 ParticleStride;
	FParticleDataContainer DataContainer;
	FVector Scale;
	int32 SortMode;

	FDynamicEmitterReplayDataBase()
		: eEmitterType(EDynamicEmitterType::Unknown)
		, ActiveParticleCount(0)
		, ParticleStride(0)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, SortMode(0)
	{
	}

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

// 스프라이트 이미터 리플레이 데이터
struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface;
	FParticleRequiredModule* RequiredModule;

	FDynamicSpriteEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, RequiredModule(nullptr)
	{
		eEmitterType = EDynamicEmitterType::Sprite;
	}

	virtual ~FDynamicSpriteEmitterReplayDataBase() = default;
};

// 메시 이미터 리플레이 데이터
struct FDynamicMeshEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface;
	class UStaticMesh* MeshData;

	FDynamicMeshEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, MeshData(nullptr)
	{
		eEmitterType = EDynamicEmitterType::Mesh;
	}

	virtual ~FDynamicMeshEmitterReplayDataBase() = default;
};

// 동적 이미터 데이터 베이스 (렌더링용)
struct FDynamicEmitterDataBase
{
	int32 EmitterIndex;

	FDynamicEmitterDataBase()
		: EmitterIndex(0)
	{
	}

	virtual ~FDynamicEmitterDataBase() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

// 스프라이트 이미터 데이터 베이스
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	virtual ~FDynamicSpriteEmitterDataBase() = default;

	virtual void SortSpriteParticles(int32 SortMode, const FVector& ViewOrigin)
	{
		// 렌더링 추가 시 구현 예정
	}

	virtual int32 GetDynamicVertexStride() const = 0;
};

// 스프라이트 이미터 데이터 구현
struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;

	virtual ~FDynamicSpriteEmitterData() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	virtual int32 GetDynamicVertexStride() const override
	{
		// sizeof(FParticleSpriteVertex) - 렌더링 구현 시 정의 예정
		return 0;
	}
};

// 메시 이미터 데이터 구현
struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	FDynamicMeshEmitterReplayDataBase MeshSource;

	virtual ~FDynamicMeshEmitterData() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return MeshSource;
	}

	virtual int32 GetDynamicVertexStride() const override
	{
		// sizeof(FMeshParticleInstanceVertex) - 렌더링 구현 시 정의 예정
		return 0;
	}
};
