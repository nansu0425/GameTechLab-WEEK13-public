#pragma once

struct FConvexElem;

class FPhysicsUtils
{
public:    
    static bool GenerateConvexHull(const TArray<FVector>& Vertices, FConvexElem& OutElem);
    
};
