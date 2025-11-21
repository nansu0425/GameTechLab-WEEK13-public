#pragma once

#include "ParticleModuleTypeDataBase.h"
#include "UParticleModuleTypeDataMesh.generated.h"

class UStaticMesh;

UCLASS(DisplayName="메시 타입 데이터", Description="메시 파티클을 위한 타입 데이터입니다")
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Mesh")
	UStaticMesh* Mesh = nullptr;

	UParticleModuleTypeDataMesh() = default;
	virtual ~UParticleModuleTypeDataMesh() = default;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
