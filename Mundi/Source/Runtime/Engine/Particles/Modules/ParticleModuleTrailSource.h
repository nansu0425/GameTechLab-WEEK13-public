#pragma once

#include "ParticleModule.h"
#include "UParticleModuleTrailSource.generated.h"

struct FBaseParticle;

/**
 * UParticleModuleTrailSource
 *
 * 다른 이미터의 파티클 위치를 추적하여 Trail을 생성합니다.
 * Ribbon 타입 이미터에서 사용하여 Sprite 파티클을 따라가는 꼬리 효과를 만듭니다.
 */
UCLASS(DisplayName="트레일 소스", Description="다른 이미터의 파티클을 추적하여 트레일을 생성합니다")
class UParticleModuleTrailSource : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 추적할 소스 이미터의 이름
	UPROPERTY(EditAnywhere, Category="Trail Source", meta=(ToolTip="추적할 소스 이미터의 이름 (EmitterName)"))
	FString SourceName = "";

	// 파티클 생성 최소 간격 (거리 기반, 너무 작으면 성능 저하, 부드러운 Ribbon은 1.0~2.0 권장)
	UPROPERTY(EditAnywhere, Category="Trail Source", meta=(ToolTip="파티클 생성 최소 거리 (Ribbon Trail은 1.0~2.0 권장)"))
	float MinSpawnDistance = 2.0f;

	// 소스 파티클의 속도를 상속할지 여부 (Ribbon Trail은 false 권장)
	UPROPERTY(EditAnywhere, Category="Trail Source", meta=(ToolTip="소스 파티클의 속도를 상속할지 여부 (Ribbon Trail은 false 권장)"))
	bool bInheritSourceVelocity = false;

	// 속도 상속 비율 (0~1)
	UPROPERTY(EditAnywhere, Category="Trail Source", meta=(ToolTip="속도 상속 비율"))
	float VelocityInheritScale = 1.0f;

public:
	UParticleModuleTrailSource()
	{
		bSpawnModule = false;
		bUpdateModule = true;  // 매 프레임 업데이트
	}

	virtual ~UParticleModuleTrailSource() = default;

	// 매 프레임 호출: 소스 파티클 추적 및 Trail 파티클 생성
	virtual void Update(FModuleUpdateContext& Context) override;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 표시 우선순위 (Spawn 다음)
	virtual int32 GetDisplayPriority() const override { return 10; }

private:
	// 마지막으로 Trail 파티클을 생성한 위치 (소스 파티클별로 추적)
	// Key: 소스 파티클 포인터, Value: 마지막 생성 위치
	TMap<FBaseParticle*, FVector> LastSpawnPositions;
};
