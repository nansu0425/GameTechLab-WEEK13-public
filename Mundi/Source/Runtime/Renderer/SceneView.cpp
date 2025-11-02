#include "pch.h"
#include "SceneView.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "Frustum.h"

FSceneView::FSceneView(ACameraActor* InCamera, FViewport* InViewport, URenderSettings* InRenderSettings)
{
    UCameraComponent* CamComp = InCamera->GetCameraComponent();
    if (!CamComp || !InViewport || !InRenderSettings)
    {
        UE_LOG("[FSceneView::FSceneView()]: CameraActor 또는 Viewport 또는 RenderSettings가 없습니다.");
        return;
    }
	RenderSettings = InRenderSettings;

    // --- 이 로직이 FSceneRenderer::PrepareView()에서 이동해 옴 ---

    float AspectRatio = 1.0f;
    if (InViewport->GetSizeY() > 0)
    {
        AspectRatio = (float)InViewport->GetSizeX() / (float)InViewport->GetSizeY();
    }

    ViewMatrix = CamComp->GetViewMatrix();
    ProjectionMatrix = CamComp->GetProjectionMatrix(AspectRatio, InViewport);
    ViewFrustum = CreateFrustumFromCamera(*CamComp, AspectRatio);
    ViewLocation = CamComp->GetWorldLocation();
    ZNear = CamComp->GetNearClip();
    ZFar = CamComp->GetFarClip();

    ViewRect.MinX = InViewport->GetStartX();
    ViewRect.MinY = InViewport->GetStartY();
    ViewRect.MaxX = ViewRect.MinX + InViewport->GetSizeX();
    ViewRect.MaxY = ViewRect.MinY + InViewport->GetSizeY();

    ProjectionMode = InCamera->GetCameraComponent()->GetProjectionMode();

    Camera = InCamera->GetCameraComponent();
    Viewport = InViewport;

    ViewShaderMacros = CreateViewShaderMacros();
}

TArray<FShaderMacro> FSceneView::CreateViewShaderMacros()
{
	TArray<FShaderMacro> ShaderMacros;

	switch (RenderSettings->GetViewMode())
	{
	case EViewMode::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewMode::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewMode::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewMode::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	case EViewMode::VMI_WorldNormal:
		ShaderMacros.push_back(FShaderMacro{ "VIEWMODE_WORLD_NORMAL", "1" });
		break;
	default:
		// 셰이더를 강제하지 않는 모드는 여기서 처리 가능
		break;
	}

	// 그림자 AA 설정
	if (RenderSettings->IsShowFlagEnabled(EEngineShowFlags::SF_ShadowAntiAliasing))
	{
		EShadowAATechnique Technique = RenderSettings->GetShadowAATechnique();
		if (Technique == EShadowAATechnique::PCF)
		{
			ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "1")); // 1 = PCF
		}
		else if (Technique == EShadowAATechnique::VSM)
		{
			ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "2")); // 2 = VSM
		}
	}
	else
	{
		ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "0")); // 0 = Hard Shadow (AA 끔)
	}

	return ShaderMacros;
}
