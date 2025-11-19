#include "pch.h"
#include "DancingCharacter.h"
#include "SkeletalMeshComponent.h"

ADancingCharacter::ADancingCharacter()
{
	if (MeshComponent)
	{
		MeshComponent->SetSkeletalMesh(GDataDir + "/CactusPA.fbx");
	}
}

ADancingCharacter::~ADancingCharacter()
{
}