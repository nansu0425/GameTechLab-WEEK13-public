#pragma once
#include "PrimitiveComponent.h"
enum class EShapeKind : uint8
{
	Box,
	Sphere,
	Capsule,
};

struct FBoxShape { FVector BoxExtent; };
struct FSphereShape { float SphereRadius; };
struct FCapsuleSphere { float CapsuleRadius, CapsuleHalfHeight; };

struct FShape
{
	FShape() {}

	EShapeKind Kind; 
	union {
		FBoxShape Box;
		FSphereShape Sphere;
		FCapsuleSphere Capsule;
	};
};
class UShapeComponent : public UPrimitiveComponent
{ 
public:  
    virtual void GetShape(FShape& OutShape) const = 0;

	virtual void OnRegister(UWorld* InWorld) override;

    // Build world-space AABB from current shape and world transform
    FAABB GetWorldAABB() const;

	UShapeComponent(); 

protected: 
	mutable FAABB WorldAABB; //브로드 페이즈 용 
	TSet<UShapeComponent*> OverlapPrev; // 지난 프레임에서 overlap 됐으면 Cache

    FVector4 ShapeColor ;

	bool bDrawOnlyIfSelected; 
	bool bHiddenInGame;
	bool bIsVisible;

	//TODO: float LineThickness;

};
