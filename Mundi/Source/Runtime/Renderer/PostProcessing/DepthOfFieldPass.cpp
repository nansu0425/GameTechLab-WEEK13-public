#include "pch.h"
#include "DepthOfFieldPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    // DoF는 FSceneView에서 직접 활성화 상태를 확인
    if (!View || !View->bEnableDepthOfField) return;

    // 1) 스왑 + SRV 언바인드 관리 (깊이 + 씬 컬러 두 장 읽음)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 3) 셰이더 로드
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* DoFPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Simple_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !DoFPS || !DoFPS->GetPixelShader())
    {
        UE_LOG("DepthOfField용 셰이더 없음!\n");
        return;
    }

    RHIDevice->PrepareShader(FullScreenTriangleVS, DoFPS);

    // 4) SRV/Sampler 설정 (깊이 + 씬 컬러)
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

    if (!DepthSRV || !SceneSRV || !PointClampSamplerState || !LinearClampSamplerState)
    {
        UE_LOG("DepthOfField: Depth SRV / Scene SRV / Sampler is null!\n");
        return;
    }

    ID3D11ShaderResourceView* Srvs[2] = { DepthSRV, SceneSRV };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

    ID3D11SamplerState* Smps[2] = { PointClampSamplerState, LinearClampSamplerState };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);

    // 5) 상수 버퍼 업데이트
    // PostProcess CB (b0) - Near/Far 클립 정보
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

    // DoF CB (b2) - DoF 파라미터 (FSceneView에서 직접 읽음)
    FDoFBufferType DoFConstant;
    DoFConstant.FocalDistance = View->DepthOfFieldFocalDistance;
    DoFConstant.CocScale = View->DepthOfFieldCocScale;
    DoFConstant.MaxBlurRadius = View->DepthOfFieldMaxBlurRadius;
    DoFConstant.NearClip = View->NearClip;
    DoFConstant.FarClip = View->FarClip;

    // 뷰포트 크기로부터 텍셀 사이즈 계산
    float ViewportWidth = static_cast<float>(View->ViewRect.Width());
    float ViewportHeight = static_cast<float>(View->ViewRect.Height());
    DoFConstant.TexelSizeX = (ViewportWidth > 0.f) ? (1.0f / ViewportWidth) : 0.f;
    DoFConstant.TexelSizeY = (ViewportHeight > 0.f) ? (1.0f / ViewportHeight) : 0.f;
    DoFConstant.Padding = 0.0f;

    RHIDevice->SetAndUpdateConstantBuffer(DoFConstant);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
