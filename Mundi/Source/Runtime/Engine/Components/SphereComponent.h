#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)
    GENERATED_REFLECTION_BODY();

    USphereComponent();
    void OnRegister(UWorld* InWorld) override;

private:
    float SphereRadius;

    void GetShape(FShape& Out) const override;

public:

    void RenderDebugVolume(class URenderer* Renderer) const override;
};
