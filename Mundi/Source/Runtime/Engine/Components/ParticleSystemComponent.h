#pragma once

#include "SceneComponent.h"
#include "Source/Runtime/Engine/Particles/ParticleSystem.h"
#include "Source/Runtime/Engine/Particles/ParticleEmitterInstance.h"
#include "UParticleSystemComponent.generated.h"

UCLASS(DisplayName="파티클 시스템 컴포넌트", Description="파티클 시스템을 씬에 배치하는 컴포넌트입니다")
class UParticleSystemComponent : public USceneComponent
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Particle System")
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere, Category="Particle System")
	bool bAutoActivate = true;

	// 이미터 인스턴스 (런타임)
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// 렌더 데이터 (렌더링 스레드용)
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	UParticleSystemComponent();
	virtual ~UParticleSystemComponent();

	// 초기화
	virtual void BeginPlay() override;
	virtual void EndPlay() override;

	// 틱
	virtual void TickComponent(float DeltaTime) override;

	// 활성화/비활성화
	void ActivateSystem();
	void DeactivateSystem();
	void ResetParticles();

	// 템플릿 설정
	void SetTemplate(UParticleSystem* NewTemplate);

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void UpdateRenderData();
};
