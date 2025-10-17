#include "pch.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(UPointLightComponent)

UPointLightComponent::UPointLightComponent()
{
	SourceRadius = 0.0f;
}

UPointLightComponent::~UPointLightComponent()
{
}

void UPointLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 점광원 특화 업데이트 로직
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "SourceRadius", SourceRadius, 0.0f);
	}
	else
	{
		InOutHandle["SourceRadius"] = SourceRadius;
	}
}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
