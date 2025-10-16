#include "pch.h"
#include "GizmoScaleComponent.h"

UGizmoScaleComponent::UGizmoScaleComponent()
{
    SetStaticMesh("Data/Gizmo/ScaleHandle.obj");
    SetMaterial("Shaders/StaticMesh/StaticMeshShader.hlsl", EVertexLayoutType::PositionColorTexturNormal);
}

UGizmoScaleComponent::~UGizmoScaleComponent()
{
}

void UGizmoScaleComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
