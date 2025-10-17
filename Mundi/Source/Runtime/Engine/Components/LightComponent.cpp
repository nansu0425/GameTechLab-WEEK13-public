#include "pch.h"
#include "LightComponent.h"

IMPLEMENT_CLASS(ULightComponent)

ULightComponent::ULightComponent()
{
	Temperature = 6500.0f;
}

ULightComponent::~ULightComponent()
{
}

FLinearColor ULightComponent::GetLightColorWithIntensity() const
{
	return LightColor * Intensity;
}

void ULightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Temperature", Temperature, 6500.0f);
	}
	else
	{
		InOutHandle["Temperature"] = Temperature;
	}
}

void ULightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
