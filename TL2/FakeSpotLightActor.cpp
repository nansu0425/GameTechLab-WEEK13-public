#include "pch.h"
#include "FakeSpotLightActor.h"
#include "DecalComponent.h"
#include "BillboardComponent.h"

AFakeSpotLightActorActor::AFakeSpotLightActorActor()
{
	Name = "Fake Spot Light Actor";
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");

	BillboardComponent->SetTextureName("Editor/SpotLight_64x.png");
	DecalComponent->SetRelativeScale((FVector(10, 5, 5)));
	DecalComponent->SetRelativeLocation((FVector(0, 0, -5)));
	DecalComponent->AddRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
	DecalComponent->SetDecalTexture("Data/FakeLight.png");

	BillboardComponent->SetupAttachment(RootComponent);
	DecalComponent->SetupAttachment(RootComponent);
}

AFakeSpotLightActorActor::~AFakeSpotLightActorActor()
{
}

void AFakeSpotLightActorActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
		{
			DecalComponent = Decal;
			break;
		}
		else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			BillboardComponent = Billboard;
			break;
		}
	}
}
