#pragma once

#include "ParticleModule.h"
#include "UParticleModuleSizeScaleBySpeed.generated.h"

UCLASS(DisplayName="크기 속도 스케일 모듈", Description="파티클의 속도에 따라 크기를 조절하는 모듈입니다")
class UParticleModuleSizeScaleBySpeed : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	// 속도를 크기로 변환하는 스케일 (X, Y축)
	// 예: SpeedScale = (0.01, 0.01)이면 속도 100일 때 크기 1.0
	UPROPERTY(EditAnywhere, Category="Size")
	FVector2D SpeedScale = FVector2D(0.0f, 0.0f);

	// 최대 크기 제한 (X, Y축)
	// 아무리 빨라도 이 크기를 넘지 않음
	UPROPERTY(EditAnywhere, Category="Size")
	FVector2D MaxScale = FVector2D(1.0f, 1.0f);

	UParticleModuleSizeScaleBySpeed()
	{
		bSpawnModule = false;   // 스폰 시 호출 안 함
		bUpdateModule = true;   // 매 프레임 업데이트 (속도 변화 추적)
	}
	virtual ~UParticleModuleSizeScaleBySpeed() = default;

	// 페이로드 필요 없음 (매 프레임 속도에서 직접 계산)
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = nullptr) override
	{
		return 0;
	}

	virtual void Update(FModuleUpdateContext& Context) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
