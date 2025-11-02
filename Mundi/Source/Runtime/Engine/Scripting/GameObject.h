#pragma once
#include "Actor.h"

class FGameObject
{
public:
    uint32  UUID;
    FString Tag;
    FVector Velocity;
    FVector Scale;
    
    void SetTag(FString NewTag) { Owner->SetTag(NewTag); }
    FString GetTag() { return Owner->GetTag(); }

    void SetLocation(FVector NewLocation) { Owner->SetActorLocation(NewLocation); }
    FVector GetLocation() { return Owner->GetActorLocation(); }

    void SetScale(FVector NewScale) { Owner->SetActorScale(NewScale); }
    FVector GetScale() { return Owner->GetActorScale(); }

    void SetRotation(FVector NewRotation)
    {
        FQuat NewQuat = FQuat::MakeFromEulerZYX(NewRotation);
        Owner->SetActorRotation(NewQuat);
    }
    FVector GetRotation() { return Owner->GetActorRotation().ToEulerZYXDeg(); }
    
    void PrintLocation()
    {
        FVector Location =  Owner->GetActorLocation();
        UE_LOG("Location %f %f %f\n", Location.X, Location.Y, Location.Z);
    }

    void SetOwner(AActor* NewOwner) { Owner = NewOwner; }
    AActor* GetOwner() { return Owner; }

private:
    // TODO : 순환 참조 해결
    AActor* Owner;
};