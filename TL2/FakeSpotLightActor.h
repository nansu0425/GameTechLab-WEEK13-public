#pragma once
#include "Actor.h"

class UBillboardComponent;
class UDecalComponent;

class AFakeSpotLightActorActor : public AActor
{
public:
	DECLARE_CLASS(AFakeSpotLightActorActor, AActor)

	AFakeSpotLightActorActor();
protected:
	~AFakeSpotLightActorActor() override;

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(AFakeSpotLightActorActor)

protected:
	UBillboardComponent* BillboardComponent{};
	UDecalComponent* DecalComponent{};
};
