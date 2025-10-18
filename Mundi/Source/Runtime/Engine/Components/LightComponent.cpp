#include "pch.h"
#include "LightComponent.h"

IMPLEMENT_CLASS(ULightComponent)

BEGIN_PROPERTIES(ULightComponent)
	MARK_AS_COMPONENT("라이트", "실제 조명 효과를 가진 라이트 컴포넌트입니다.")
	ADD_PROPERTY_RANGE(float, Temperature, "Light", 1000.0f, 15000.0f, true, 
		"조명의 색온도를 켈빈(K) 단위로 설정합니다\n(1000K: 주황색, 6500K: 주광색, 15000K: 푸른색)")
END_PROPERTIES()

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

	// 리플렉션 기반 자동 직렬화
	AutoSerialize(bInIsLoading, InOutHandle, ULightComponent::StaticClass());
}

void ULightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
