#pragma once
#include "LightComponent.h"

// 환경광 (전역 조명)
class UAmbientLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

	UAmbientLightComponent();
	virtual ~UAmbientLightComponent() override;

public:
	// Virtual Interface
	virtual void UpdateLightData() override;

	// Serialization & Duplication
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UAmbientLightComponent)
};
