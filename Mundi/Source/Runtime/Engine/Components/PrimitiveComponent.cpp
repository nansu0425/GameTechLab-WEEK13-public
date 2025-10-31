#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"

IMPLEMENT_CLASS(UPrimitiveComponent)


void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void AActor::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
}
void AActor::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
}
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::OnSerialized()
{
    Super::OnSerialized();
}
