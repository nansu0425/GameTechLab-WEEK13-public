#pragma once

#include "ParticleModule.h"
#include "UParticleModuleColor.generated.h"

// 언리얼 엔진 호환: 페이로드 시스템 - 색상 모듈
// 파티클별 색상 데이터를 저장 (랜덤 색상, OverLife 최적화 등)
struct FParticleColorPayload
{
	FLinearColor InitialColor;    // 생성 시 초기 색상 (랜덤 적용 후)
	FLinearColor TargetColor;     // 목표 색상 (보간용)
	float ColorChangeRate;        // 색상 변화 속도 (OverLife 최적화)
	float Padding;                // 16바이트 정렬 유지 (48바이트 총 크기)
};

UCLASS(DisplayName="색상 모듈", Description="파티클의 색상을 정의하며, 시간에 따른 색상 변화를 설정할 수 있습니다")
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Color")
	FLinearColor StartColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Color")
	FLinearColor EndColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 언리얼 엔진 호환: 색상 랜덤화
	UPROPERTY(EditAnywhere, Category="Color")
	float ColorRandomness = 0.0f;  // 0.0 = 랜덤 없음, 1.0 = 완전 랜덤

	UParticleModuleColor()
	{
		bSpawnModule = true;   // 스폰 시 초기 색상 설정
		bUpdateModule = true;  // 매 프레임 색상 보간
	}
	virtual ~UParticleModuleColor() = default;

	// 언리얼 엔진 호환: 페이로드 시스템
	virtual uint32 RequiredBytes(FParticleEmitterInstance* Owner = nullptr) override;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FModuleUpdateContext& Context) override;
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
