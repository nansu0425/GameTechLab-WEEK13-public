#pragma once
#include "PrimitiveComponent.h"

class UShader;

class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UMeshComponent)

    bool IsCastShadows() { return bCastShadows; }

private:
    UPROPERTY(EditAnywhere, Category="Rendering", Tooltip="그림자를 드리울지 여부입니다")
    bool bCastShadows = true;
};