#include "pch.h"
#include "ShapeComponent.h"
#include "OBB.h"

UShapeComponent::UShapeComponent()
{
    ShapeColor = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
}

 
void UShapeComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
    
    GetWorldAABB();

    UpdateOverlaps();
}

void UShapeComponent::UpdateOverlaps()
{
    if (!bGenerateOverlapEvents)
    {
        OverlapInfos.clear();
        return;
    }
    
    TSet<UShapeComponent*> Now;

    // Broad Phase

    // Narrow Phase

    //Now.Add();

    //Begin
    //for( UShapeComponent* S : Now) 
    //OnComponentBeginOverlap.Broadcast(this, S);

    //for( UShapeComponent* S: Prev)
    //OnComponentEndOverlap.Broadcast(this, S);

    //공개 리스트 갱신
    //OverlapInfo <= Now
    //Prec <= Now
}

FAABB UShapeComponent::GetWorldAABB() const
{
    if (AActor* Owner = GetOwner())
    {
        FAABB OwnerBounds = Owner->GetBounds();
        const FVector HalfExtent = OwnerBounds.GetHalfExtent();
        WorldAABB = OwnerBounds; 
    }
    return WorldAABB; 
}