#pragma once

#include "Object.h"
#include "ParticleHelper.h"
#include "UParticleModule.generated.h"

struct FParticleEmitterInstance;

UCLASS(DisplayName="파티클 모듈", Description="파티클 이미터의 동작을 정의하는 베이스 모듈입니다")
class UParticleModule : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Particle Module")
	bool bEnabled = true;

	UParticleModule() = default;
	virtual ~UParticleModule() = default;

	// 이미터 인스턴스가 생성될 때 한 번 호출
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
	{
		// 파생 클래스에서 오버라이드
	}

	// 매 프레임마다 파티클 업데이트를 위해 호출
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
	{
		// 파생 클래스에서 오버라이드
	}

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
