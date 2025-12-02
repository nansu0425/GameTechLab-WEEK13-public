#pragma once
#include "PostProcessing.h"

class UShader;

// Depth of Field 포스트 프로세스 패스 (Phase 2 - 4패스 분리형)
// Pass 1: Downsample + CoC (MRT)
// Pass 2: Horizontal Blur
// Pass 3: Vertical Blur
// Pass 4: Upsample + Composite
class FDepthOfFieldPass final : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

private:
    // 셰이더 로드 (캐싱)
    bool LoadShaders();

    // 상수 버퍼 업데이트 헬퍼
    void UpdateDoFConstantBuffer(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M, float BlurDirection);

    // SRV 언바인드 헬퍼 (SRV-RTV 충돌 방지)
    void ClearPSSRVs(D3D11RHI* RHIDevice, UINT StartSlot, UINT NumSlots);

    // 4패스 개별 실행
    void ExecutePass1_Downsample(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M);
    void ExecutePass2_HorizontalBlur(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M);
    void ExecutePass3_VerticalBlur(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M);
    void ExecutePass4_Upsample(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M);

    // 캐시된 셰이더
    UShader* DoFFullScreenVS = nullptr;  // DoF 전용 VS (FViewportConstants 미사용)
    UShader* DownsamplePS = nullptr;
    UShader* HorizontalBlurPS = nullptr;
    UShader* VerticalBlurPS = nullptr;
    UShader* UpsamplePS = nullptr;

    bool bShadersLoaded = false;
};
