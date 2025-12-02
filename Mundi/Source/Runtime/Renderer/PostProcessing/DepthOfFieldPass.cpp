#include "pch.h"
#include "DepthOfFieldPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

// ===== 셰이더 로드 (캐싱) =====
bool FDepthOfFieldPass::LoadShaders()
{
    if (bShadersLoaded) return true;

    // DoF 전용 VS 로드 (FViewportConstants 미사용, 단순 0~1 UV 출력)
    DoFFullScreenVS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_FullScreen_VS.hlsl");
    DownsamplePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Downsample_PS.hlsl");
    HorizontalBlurPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_HorizontalBlur_PS.hlsl");
    VerticalBlurPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_VerticalBlur_PS.hlsl");
    UpsamplePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Upsample_PS.hlsl");

    if (!DoFFullScreenVS || !DoFFullScreenVS->GetVertexShader())
    {
        UE_LOG("DoF Phase 2: Failed to load DoF_FullScreen_VS");
        return false;
    }
    if (!DownsamplePS || !DownsamplePS->GetPixelShader())
    {
        UE_LOG("DoF Phase 2: Failed to load DoF_Downsample_PS");
        return false;
    }
    if (!HorizontalBlurPS || !HorizontalBlurPS->GetPixelShader())
    {
        UE_LOG("DoF Phase 2: Failed to load DoF_HorizontalBlur_PS");
        return false;
    }
    if (!VerticalBlurPS || !VerticalBlurPS->GetPixelShader())
    {
        UE_LOG("DoF Phase 2: Failed to load DoF_VerticalBlur_PS");
        return false;
    }
    if (!UpsamplePS || !UpsamplePS->GetPixelShader())
    {
        UE_LOG("DoF Phase 2: Failed to load DoF_Upsample_PS");
        return false;
    }

    bShadersLoaded = true;
    return true;
}

// ===== 상수 버퍼 업데이트 =====
void FDepthOfFieldPass::UpdateDoFConstantBuffer(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M, float BlurDirection)
{
    // ViewRect는 3D 씬 렌더링 영역 (ImGui 제외)
    float ViewportWidth = static_cast<float>(View->ViewRect.Width());
    float ViewportHeight = static_cast<float>(View->ViewRect.Height());
    float HalfWidth = ViewportWidth * 0.5f;
    float HalfHeight = ViewportHeight * 0.5f;

    // Full-res 렌더 타겟 크기 (ImGui 포함 전체)
    float FullRTWidth = static_cast<float>(RHIDevice->GetViewportWidth());
    float FullRTHeight = static_cast<float>(RHIDevice->GetViewportHeight());

    // Full-res 텍스처에서 ViewRect 영역의 UV 오프셋 및 스케일
    // Downsample 패스에서 Full-res 텍스처의 올바른 영역만 샘플링하기 위함
    float SourceUVOffsetX = static_cast<float>(View->ViewRect.MinX) / FullRTWidth;
    float SourceUVOffsetY = static_cast<float>(View->ViewRect.MinY) / FullRTHeight;
    float SourceUVScaleX = ViewportWidth / FullRTWidth;
    float SourceUVScaleY = ViewportHeight / FullRTHeight;

    FDoFBufferType DoFConstant;
    DoFConstant.FocalDistance = M.Payload.Params0.X;
    DoFConstant.CocScale = M.Payload.Params0.Y;
    DoFConstant.MaxBlurRadius = M.Payload.Params0.Z;
    DoFConstant.NearClip = View->NearClip;
    DoFConstant.FarClip = View->FarClip;
    DoFConstant.TexelSizeX = (ViewportWidth > 0.f) ? (1.0f / ViewportWidth) : 0.f;
    DoFConstant.TexelSizeY = (ViewportHeight > 0.f) ? (1.0f / ViewportHeight) : 0.f;
    DoFConstant.HalfTexelSizeX = (HalfWidth > 0.f) ? (1.0f / HalfWidth) : 0.f;
    DoFConstant.HalfTexelSizeY = (HalfHeight > 0.f) ? (1.0f / HalfHeight) : 0.f;
    DoFConstant.BlurDirection = BlurDirection;
    DoFConstant.SourceUVOffsetX = SourceUVOffsetX;
    DoFConstant.SourceUVOffsetY = SourceUVOffsetY;
    DoFConstant.SourceUVScaleX = SourceUVScaleX;
    DoFConstant.SourceUVScaleY = SourceUVScaleY;
    DoFConstant.Padding5 = 0.0f;
    DoFConstant.Padding6 = 0.0f;

    RHIDevice->SetAndUpdateConstantBuffer(DoFConstant);
}

// ===== SRV 언바인드 헬퍼 =====
void FDepthOfFieldPass::ClearPSSRVs(D3D11RHI* RHIDevice, UINT StartSlot, UINT NumSlots)
{
    ID3D11ShaderResourceView* NullSRVs[8] = { nullptr };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(StartSlot, NumSlots, NullSRVs);
}

// ===== Pass 1: Downsample + CoC (MRT) =====
void FDepthOfFieldPass::ExecutePass1_Downsample(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M)
{
    auto* DC = RHIDevice->GetDeviceContext();

    // ★ Full-res SceneColor Swap: 직전 패스(LitPath 또는 이전 포스트프로세스)가 Target에 그린 것을
    // Source로 전환하여 읽을 수 있게 함. Pass 1은 Half-res 버퍼에 그리므로 Commit하지 않음.
    // 스코프 끝에서 자동 롤백되어 Full-res 버퍼 상태가 원래대로 유지됨.
    FSwapGuard FullResSwap(RHIDevice, 0, 2);

    // Half-res 뷰포트로 전환
    RHIDevice->SetHalfResViewport();

    // DoF 전용 VS는 FViewportConstants를 사용하지 않으므로 상수 버퍼 설정 불필요
    // UV는 PS에서 RemapToSourceUV로 명시적 변환

    // MRT 설정: HalfResColor[0] + HalfResCoC
    ID3D11RenderTargetView* RTVs[2] = {
        RHIDevice->GetHalfResColorTargetRTV(),  // Half-res Color (Target = index 1)
        RHIDevice->GetHalfResCoCRTV()           // Half-res CoC
    };
    DC->OMSetRenderTargets(2, RTVs, nullptr);

    // 셰이더 설정 (DoF 전용 VS 사용)
    RHIDevice->PrepareShader(DoFFullScreenVS, DownsamplePS);

    // SRV 설정: Full-res SceneColor + Full-res Depth
    // FSwapGuard로 Swap했으므로 GetCurrentSourceSRV()가 직전 패스 결과를 반환
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetCurrentSourceSRV();
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SRVs[2] = { SceneSRV, DepthSRV };
    DC->PSSetShaderResources(0, 2, SRVs);

    // Sampler 설정
    ID3D11SamplerState* LinearSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* Samplers[2] = { LinearSampler, PointSampler };
    DC->PSSetSamplers(0, 2, Samplers);

    // 상수 버퍼 업데이트
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));
    UpdateDoFConstantBuffer(RHIDevice, View, M, 0.0f);

    // Draw
    RHIDevice->DrawFullScreenQuad();

    // SRV 언바인드는 FSwapGuard 소멸자에서 자동 처리됨

    // Ping-Pong 스왑 (다음 패스에서 방금 그린 것을 Source로 사용)
    RHIDevice->SwapHalfResRenderTargets();

    // FullResSwap은 Commit 없이 소멸 → Full-res SceneColor 버퍼 상태 자동 롤백
}

// ===== Pass 2: Horizontal Blur =====
void FDepthOfFieldPass::ExecutePass2_HorizontalBlur(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M)
{
    auto* DC = RHIDevice->GetDeviceContext();

    // RTV 설정: HalfResColor[Target]
    ID3D11RenderTargetView* RTV = RHIDevice->GetHalfResColorTargetRTV();
    DC->OMSetRenderTargets(1, &RTV, nullptr);

    // 셰이더 설정 (DoF 전용 VS 사용)
    RHIDevice->PrepareShader(DoFFullScreenVS, HorizontalBlurPS);

    // SRV 설정: HalfResColor[Source] + HalfResCoC
    ID3D11ShaderResourceView* ColorSRV = RHIDevice->GetHalfResColorSourceSRV();
    ID3D11ShaderResourceView* CoCRV = RHIDevice->GetHalfResCoCSRV();
    ID3D11ShaderResourceView* SRVs[2] = { ColorSRV, CoCRV };
    DC->PSSetShaderResources(0, 2, SRVs);

    // Sampler 설정
    ID3D11SamplerState* LinearSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* Samplers[2] = { LinearSampler, PointSampler };
    DC->PSSetSamplers(0, 2, Samplers);

    // 상수 버퍼 업데이트 (BlurDirection = 0.0 for Horizontal)
    UpdateDoFConstantBuffer(RHIDevice, View, M, 0.0f);

    // Draw
    RHIDevice->DrawFullScreenQuad();

    // SRV 언바인드
    ClearPSSRVs(RHIDevice, 0, 2);

    // Ping-Pong 스왑
    RHIDevice->SwapHalfResRenderTargets();
}

// ===== Pass 3: Vertical Blur =====
void FDepthOfFieldPass::ExecutePass3_VerticalBlur(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M)
{
    auto* DC = RHIDevice->GetDeviceContext();

    // RTV 설정: HalfResColor[Target]
    ID3D11RenderTargetView* RTV = RHIDevice->GetHalfResColorTargetRTV();
    DC->OMSetRenderTargets(1, &RTV, nullptr);

    // 셰이더 설정 (DoF 전용 VS 사용)
    RHIDevice->PrepareShader(DoFFullScreenVS, VerticalBlurPS);

    // SRV 설정: HalfResColor[Source] + HalfResCoC
    ID3D11ShaderResourceView* ColorSRV = RHIDevice->GetHalfResColorSourceSRV();
    ID3D11ShaderResourceView* CoCRV = RHIDevice->GetHalfResCoCSRV();
    ID3D11ShaderResourceView* SRVs[2] = { ColorSRV, CoCRV };
    DC->PSSetShaderResources(0, 2, SRVs);

    // Sampler 설정
    ID3D11SamplerState* LinearSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* Samplers[2] = { LinearSampler, PointSampler };
    DC->PSSetSamplers(0, 2, Samplers);

    // 상수 버퍼 업데이트 (BlurDirection = 1.0 for Vertical)
    UpdateDoFConstantBuffer(RHIDevice, View, M, 1.0f);

    // Draw
    RHIDevice->DrawFullScreenQuad();

    // SRV 언바인드
    ClearPSSRVs(RHIDevice, 0, 2);

    // Ping-Pong 스왑 (최종 블러 결과가 Source에 위치)
    RHIDevice->SwapHalfResRenderTargets();
}

// ===== Pass 4: Upsample + Composite =====
void FDepthOfFieldPass::ExecutePass4_Upsample(D3D11RHI* RHIDevice, FSceneView* View, const FPostProcessModifier& M)
{
    auto* DC = RHIDevice->GetDeviceContext();

    // ViewRect 기반 Full-res 뷰포트로 복원 (전체 RT가 아닌 씬 렌더링 영역)
    D3D11_VIEWPORT FullResVP = {};
    FullResVP.TopLeftX = static_cast<float>(View->ViewRect.MinX);
    FullResVP.TopLeftY = static_cast<float>(View->ViewRect.MinY);
    FullResVP.Width = static_cast<float>(View->ViewRect.Width());
    FullResVP.Height = static_cast<float>(View->ViewRect.Height());
    FullResVP.MinDepth = 0.0f;
    FullResVP.MaxDepth = 1.0f;
    DC->RSSetViewports(1, &FullResVP);

    // DoF 전용 VS는 FViewportConstants를 사용하지 않으므로 상수 버퍼 설정 불필요
    // UV는 PS에서 RemapToHalfResUV로 명시적 변환

    // FSwapGuard로 Full-res SceneColor Ping-Pong 관리
    FSwapGuard Swap(RHIDevice, 0, 3);

    // RTV 설정: Full-res SceneColor[Target]
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // 셰이더 설정 (DoF 전용 VS 사용)
    RHIDevice->PrepareShader(DoFFullScreenVS, UpsamplePS);

    // SRV 설정: Full-res Sharp + Half-res Blur + Full-res Depth
    ID3D11ShaderResourceView* SharpSRV = RHIDevice->GetCurrentSourceSRV();
    ID3D11ShaderResourceView* BlurSRV = RHIDevice->GetHalfResColorSourceSRV();
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SRVs[3] = { SharpSRV, BlurSRV, DepthSRV };
    DC->PSSetShaderResources(0, 3, SRVs);

    // Sampler 설정
    ID3D11SamplerState* LinearSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* Samplers[2] = { LinearSampler, PointSampler };
    DC->PSSetSamplers(0, 2, Samplers);

    // 상수 버퍼 업데이트
    UpdateDoFConstantBuffer(RHIDevice, View, M, 0.0f);

    // Draw
    RHIDevice->DrawFullScreenQuad();

    // SRV 언바인드
    ClearPSSRVs(RHIDevice, 0, 3);

    // 스왑 확정
    Swap.Commit();
}

// ===== 메인 Execute =====
void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    // Modifier 유효성 검사 (bEnabled, Weight > 0)
    if (!IsApplicable(M) || !View) return;

    // 셰이더 로드
    if (!LoadShaders()) return;

    // ViewRect 기반 Half-res 버퍼 크기 확인 및 재생성
    // 핵심 수정: 전체 RT가 아닌 ViewRect 크기의 절반으로 Half-res 버퍼 생성
    UINT RequiredHalfWidth = View->ViewRect.Width() / 2;
    UINT RequiredHalfHeight = View->ViewRect.Height() / 2;

    if (RHIDevice->GetHalfResWidth() != RequiredHalfWidth ||
        RHIDevice->GetHalfResHeight() != RequiredHalfHeight)
    {
        // ViewRect 크기의 2배를 전달하면 CreateHalfResolutionBuffers가 절반으로 나눔
        RHIDevice->CreateHalfResolutionBuffers(View->ViewRect.Width(), View->ViewRect.Height());
    }

    // 공통 렌더 스테이트 설정
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // PostProcess CB (b0) 업데이트 - 모든 패스에서 공통 사용
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

    // ===== 4 Pass DoF Pipeline =====
    // Pass 1: Full-res → Half-res (Downsample + CoC)
    ExecutePass1_Downsample(RHIDevice, View, M);

    // Pass 2: Half-res Horizontal Blur
    ExecutePass2_HorizontalBlur(RHIDevice, View, M);

    // Pass 3: Half-res Vertical Blur
    ExecutePass3_VerticalBlur(RHIDevice, View, M);

    // Pass 4: Half-res → Full-res (Upsample + Composite)
    ExecutePass4_Upsample(RHIDevice, View, M);
}
