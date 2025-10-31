#pragma once
struct FAABB;
struct FOBB;
struct FBoundingSphere;
struct FShape;

class UShapeComponent;

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb);

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere);

	bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere);

    using OverlapFunc = bool(*) (const FShape&, const FTransform&, const FShape&, const FTransform&);
    bool OverlapSphereAndSphere(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB);

    extern OverlapFunc OverlapLUT[3][3];
    
    
    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B);

}