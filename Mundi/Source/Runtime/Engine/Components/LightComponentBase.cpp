#include "pch.h"
#include "LightComponentBase.h"

IMPLEMENT_CLASS(ULightComponentBase)

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
