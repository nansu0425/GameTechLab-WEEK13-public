#pragma once
#include "LightComponent.h"

// 위치 기반 로컬 라이트의 공통 베이스 (Point, Spot)
class ULocalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)

	ULocalLightComponent();
	virtual ~ULocalLightComponent() override;

public:
	// Attenuation Properties
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }
	float GetAttenuationRadius() const { return AttenuationRadius; }

	void SetFalloffExponent(float InExponent) { FalloffExponent = InExponent; }
	float GetFalloffExponent() const { return FalloffExponent; }

	// 감쇠 계산
	virtual float GetAttenuationFactor(const FVector& WorldPosition) const;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULocalLightComponent)

protected:
	float AttenuationRadius = 1000.0f; // 감쇠 반경
	float FalloffExponent = 1.0f;      // 감쇠 지수
};
