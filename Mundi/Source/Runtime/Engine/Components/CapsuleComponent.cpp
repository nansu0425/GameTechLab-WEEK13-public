#include "pch.h"
#include "CapsuleComponent.h"

IMPLEMENT_CLASS(UCapsuleComponent)

UCapsuleComponent::UCapsuleComponent()
{
	CapsuleHalfHeight = WorldAABB.GetHalfExtent().Z;
	CapsuleRadius = FMath::Max(WorldAABB.GetHalfExtent().X, WorldAABB.GetHalfExtent().Y);
}

void UCapsuleComponent::GetShape(FShape& Out) const
{
	Out.Kind = EShapeKind::Capsule;
	Out.Capsule.CapsuleHalfHeight = CapsuleHalfHeight;
	Out.Capsule.CapsuleRadius = CapsuleRadius;
}
