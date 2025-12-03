#include "pch.h"
#include "PhysicsUtils.h"
#include "Shape/ConvexElem.h"
#include "PhysicsCore.h"
#include "PhysicsTypes.h"

using namespace physx;

bool FPhysicsUtils::GenerateConvexHull(const TArray<FVector>& Vertices, FConvexElem& OutElem)
{
    if (Vertices.IsEmpty())
    {
        return false;
    }

    PxPhysics* Physics = FPhysicsCore::Get().GetPhysics();
    PxCooking* Cooking = FPhysicsCore::Get().GetCooking();
    if (!Physics || !Cooking)
    {
        return false;
    }

    TArray<PxVec3> PxVertices;
    PxVertices.Reserve(Vertices.Num());
    for (const auto& Vertex : Vertices)
    {
        PxVertices.Add(PhysicsConversion::ToPxVec3(Vertex));
    }

    PxConvexMeshDesc ConvexDesc;
    ConvexDesc.points.count = PxVertices.Num();
    ConvexDesc.points.data = PxVertices.GetData();
    ConvexDesc.points.stride = sizeof(PxVec3);
    ConvexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
    ConvexDesc.vertexLimit = 256;

    PxDefaultMemoryOutputStream Buf;
    PxConvexMeshCookingResult::Enum Result;
    if (!Cooking->cookConvexMesh(ConvexDesc, Buf, &Result))
    {
        UE_LOG("GenerateConvexHull - Cooking failed");
        return false;
    }

    PxDefaultMemoryInputData Input(Buf.getData(), Buf.getSize());
    physx::PxConvexMesh* TempMesh = Physics->createConvexMesh(Input);

    if (!TempMesh)
    {
        return false;
    }

    const PxU32 HullVertexCount = TempMesh->getNbVertices();
    const PxVec3* HullVertices = TempMesh->getVertices();

    TArray<FVector> OptimizedVertices;
    OptimizedVertices.Reserve(HullVertexCount);

    for (PxU32 i = 0; i < HullVertexCount; i++)
    {
        OptimizedVertices.Add(PhysicsConversion::ToFVector(HullVertices[i]));
    }
    OutElem.SetVertexData(OptimizedVertices);

    const PxU8* PxIndices = TempMesh->getIndexBuffer(); // 인덱스 버퍼 포인터
    const PxU32 NbPolygons = TempMesh->getNbPolygons();
    
    TArray<int32> OptimizedIndices;
    for (PxU32 i = 0; i < NbPolygons; i++)
    {
        PxHullPolygon PolyData;
        TempMesh->getPolygonData(i, PolyData);

        // PolyData.mIndexBase: 이 폴리곤의 시작 인덱스가 PxIndices 배열 어디에 있는지
        // PolyData.mNbVerts: 이 폴리곤을 구성하는 정점 개수 (3=삼각형, 4=사각형...)
        const PxU32 BaseOffset = PolyData.mIndexBase;
        const int32 NumVerts = PolyData.mNbVerts;

        // [Triangle Fan 알고리즘]
        // n각형을 (n-2)개의 삼각형으로 부채꼴처럼 쪼갭니다.
        // 기준점(0번)을 잡고 2, 3, ... 순서로 연결합니다.
        for (int32 j = 2; j < NumVerts; j++)
        {
            // PhysX(RH) -> DirectX(LH) 변환 시 와인딩 순서(Winding Order) 고려
            // 만약 렌더링 시 면이 뒤집혀 보인다면 (0, j, j-1)로 순서를 바꾸세요.
            
            // Triangle 1: (Base, Base+1, Base+2) ...
            OptimizedIndices.Add(PxIndices[BaseOffset + 0]);     // 기준점
            OptimizedIndices.Add(PxIndices[BaseOffset + j]); // 이전 점
            OptimizedIndices.Add(PxIndices[BaseOffset + j - 1]);     // 현재 점
        }
    }

    OutElem.IndexData = OptimizedIndices;
    
    TempMesh->release();

    return true;
}
