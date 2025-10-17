#include "pch.h"
#include "LightComponentBase.h"

IMPLEMENT_CLASS(ULightComponentBase)

// 리플렉션 프로퍼티 등록
void ULightComponentBase::StaticRegisterProperties()
{
	UClass* Class = StaticClass();

	// 컴포넌트 메타데이터 설정
	MARK_AS_COMPONENT("Light Component Base", "Base class for all light components")

	// 프로퍼티 등록
	ADD_PROPERTY(bool, bIsEnabled, "Light");
	ADD_PROPERTY_RANGE(float, Intensity, "Light", 0.0f, 100.0f);
	ADD_PROPERTY(FLinearColor, LightColor, "Light");
}

ULightComponentBase::ULightComponentBase()
{
	bIsEnabled = true;
	Intensity = 1.0f;
	LightColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

ULightComponentBase::~ULightComponentBase()
{
}

void ULightComponentBase::UpdateLightData()
{
	// 자식 클래스에서 오버라이드
}

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadBool(InOutHandle, "bIsEnabled", bIsEnabled, true);
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 1.0f);

		if (InOutHandle.hasKey("LightColor"))
		{
			FVector4 ColorVec;
			FJsonSerializer::ReadVector4(InOutHandle, "LightColor", ColorVec);
			LightColor = FLinearColor(ColorVec);
		}
	}
	else
	{
		InOutHandle["bIsEnabled"] = bIsEnabled;
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["LightColor"] = FJsonSerializer::Vector4ToJson(LightColor.ToFVector4());
	}
}

void ULightComponentBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
