#pragma once
#include "PostProcessing.h"

// Depth of Field 포스트 프로세스 패스 (Phase 1 - 단일 패스 간소화 버전)
class FDepthOfFieldPass final : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;
};
