#include "pch.h"
#include "FakeSpotLightActor.h"
#include "PerspectiveDecalComponent.h"
#include "BillboardComponent.h"

AFakeSpotLightActor::AFakeSpotLightActor()
{
	Name = "Fake Spot Light Actor";
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	DecalComponent = CreateDefaultSubobject<UPerspectiveDecalComponent>("DecalComponent");

	BillboardComponent->SetTextureName("Editor/SpotLight_64x.png");
	DecalComponent->SetRelativeScale((FVector(10, 5, 5)));
	DecalComponent->SetRelativeLocation((FVector(0, 0, -5)));
	DecalComponent->AddRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
	DecalComponent->SetDecalTexture("Data/FakeLight.png");
	DecalComponent->SetFovY(60);

	BillboardComponent->SetupAttachment(RootComponent);
	DecalComponent->SetupAttachment(RootComponent);
}

AFakeSpotLightActor::~AFakeSpotLightActor()
{
}

void AFakeSpotLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UPerspectiveDecalComponent* Decal = Cast<UPerspectiveDecalComponent>(Component))
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
