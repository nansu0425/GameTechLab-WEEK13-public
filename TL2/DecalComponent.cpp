#include "pch.h"
#include <cmath>
#include "DecalComponent.h"
#include "OBB.h"

void UDecalComponent::Serialize(bool bIsLoading, FDecalData& InOut)
{
}

void UDecalComponent::DuplicateSubObjects()
{
    
}

void UDecalComponent::RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj)
{
    // TODO: 실제 렌더 부분
    //Renderer->GetRHIDevice()->Update
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
	// 로컬 단위 큐브의 정점과 선 정보 정의 (위와 동일)
	const FVector4 LocalVertices[8] = {
		FVector4(-0.5f, -0.5f, -0.5f, 1.0f), FVector4(0.5f, -0.5f, -0.5f, 1.0f),
		FVector4(0.5f, 0.5f, -0.5f, 1.0f), FVector4(-0.5f, 0.5f, -0.5f, 1.0f),
		FVector4(-0.5f, -0.5f, 0.5f, 1.0f), FVector4(0.5f, -0.5f, 0.5f, 1.0f),
		FVector4(0.5f, 0.5f, 0.5f, 1.0f), FVector4(-0.5f, 0.5f, 0.5f, 1.0f)
	};

	const int Edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // 하단
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // 상단
		{0, 4}, {1, 5}, {2, 6}, {3, 7}  // 기둥
	};

	// 컴포넌트의 월드 변환 행렬
	const FMatrix WorldMatrix = GetWorldMatrix();

	// 라인 색상
	const FVector4 BoxColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

	// AddLines 함수에 전달할 데이터 배열들을 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 12개의 선 데이터를 배열에 채워 넣습니다.
	for (int i = 0; i < 12; ++i)
	{
		// 월드 좌표로 변환
		const FVector4 WorldStart = (LocalVertices[Edges[i][0]]) * WorldMatrix;
		const FVector4 WorldEnd = (LocalVertices[Edges[i][1]]) * WorldMatrix;

		StartPoints.Add(FVector(WorldStart.X, WorldStart.Y, WorldStart.Z));
		EndPoints.Add(FVector(WorldEnd.X, WorldEnd.Y, WorldEnd.Z));
		Colors.Add(BoxColor);
	}

	// 모든 데이터를 준비한 뒤, 단 한 번의 호출로 렌더러에 전달합니다.
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
}

FAABB UDecalComponent::GetWorldAABB() const
{
    // Step 1: Build the decal's oriented box so we can inspect its world-space corners.
    const FOBB DecalOBB = GetOBB();

    // Step 2: Initialize min/max accumulators that will grow to the final axis-aligned bounds.
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(FLT_MAX, FLT_MAX, FLT_MAX);

    // Step 3: Evaluate all 8 OBB corners in world-space.
    const FVector& Center = DecalOBB.Center;
    const FVector& HalfExtent = DecalOBB.HalfExtent;
    const FVector (&Axes)[3] = DecalOBB.Axes;

    for (uint8 sx = -1; sx <= 1; sx += 2)
    {
        for (uint8 sy = -1; sy <= 1; sy += 2)
        {
            for (uint8 sz = -1; sz <= 1; sz += 2)
            {
                const FVector Corner = Center
                    + Axes[0] * (HalfExtent.X * static_cast<float>(sx))
                    + Axes[1] * (HalfExtent.Y * static_cast<float>(sy))
                    + Axes[2] * (HalfExtent.Z * static_cast<float>(sz));

                MinBounds.X = std::min(MinBounds.X, Corner.X);
                MinBounds.Y = std::min(MinBounds.Y, Corner.Y);
                MinBounds.Z = std::min(MinBounds.Z, Corner.Z);

                MaxBounds.X = std::max(MaxBounds.X, Corner.X);
                MaxBounds.Y = std::max(MaxBounds.Y, Corner.Y);
                MaxBounds.Z = std::max(MaxBounds.Z, Corner.Z);
            }
        }
    }

    // Step 4: Package the accumulated extremes into a world-space AABB.
    return FAABB(MinBounds, MaxBounds);
}

FOBB UDecalComponent::GetOBB() const
{
    // Step 1: Grab the decal's world transform so we can inspect translation, rotation, and scale.
    const FTransform WorldTransform = GetWorldTransform();

    // Step 2: The translation directly defines the OBB center in world-space.
    const FVector Center = WorldTransform.Translation;

    // Step 3: Treat the scale as the full size on each axis and convert it into half extents.
    const FVector FullExtent(
        static_cast<float>(std::fabs(WorldTransform.Scale3D.X)),
        static_cast<float>(std::fabs(WorldTransform.Scale3D.Y)),
        static_cast<float>(std::fabs(WorldTransform.Scale3D.Z)));
    const FVector HalfExtent = FullExtent * 0.5f;

    // Step 4: Build orthonormal axes from the world rotation, preserving mirroring caused by negative scale.
    const FQuat WorldRotation = WorldTransform.Rotation.GetNormalized();
    const float SignX = (WorldTransform.Scale3D.X >= 0.0f) ? 1.0f : -1.0f;
    const float SignY = (WorldTransform.Scale3D.Y >= 0.0f) ? 1.0f : -1.0f;
    const float SignZ = (WorldTransform.Scale3D.Z >= 0.0f) ? 1.0f : -1.0f;

    FVector Axes[3];
    Axes[0] = WorldRotation.RotateVector(FVector(SignX, 0.0f, 0.0f)).GetSafeNormal();
    Axes[1] = WorldRotation.RotateVector(FVector(0.0f, SignY, 0.0f)).GetSafeNormal();
    Axes[2] = WorldRotation.RotateVector(FVector(0.0f, 0.0f, SignZ)).GetSafeNormal();

    // Step 5: Assemble the final OBB description.
    return FOBB(Center, HalfExtent, Axes);
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
    return FMatrix();
}




