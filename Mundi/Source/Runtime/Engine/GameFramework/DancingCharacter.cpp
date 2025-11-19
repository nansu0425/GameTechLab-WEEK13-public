#include "pch.h"
#include "DancingCharacter.h"
#include "SkeletalMeshComponent.h"
#include "AudioComponent.h"

ADancingCharacter::ADancingCharacter()
{
	if (MeshComponent)
	{
		MeshComponent->SetSkeletalMesh(GDataDir + "/SillyDancing.fbx");
	}

	AudioComponent = CreateDefaultSubobject<UAudioComponent>("AudioComponent");
	if (AudioComponent)
	{
		AudioComponent->SetupAttachment(RootComponent);
	}
}

ADancingCharacter::~ADancingCharacter()
{
}

void ADancingCharacter::BeginPlay()
{
	Super::BeginPlay();

	UAnimSequence* AnimToPlay = UResourceManager::GetInstance().Get<UAnimSequence>(GDataDir + "/SillyDancing_mixamo.com");

	if (AnimToPlay && GetMesh())
	{
		MeshComponent->PlayAnimation(AnimToPlay, true);
		UE_LOG("ADancingCharacter: Started playing animation.");
	}
	else
	{
		UE_LOG("ADancingCharacter: Failedd to find animation to play");
	}
}

void ADancingCharacter::HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
	Super::HandleAnimNotify(NotifyEvent);

	UE_LOG("ADancingCharacter: Notify Triggered!");
	if (AudioComponent)
	{
		AudioComponent->Play();
	}
}