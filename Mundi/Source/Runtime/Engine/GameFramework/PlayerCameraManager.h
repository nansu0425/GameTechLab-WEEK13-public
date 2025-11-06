#pragma once
#include "Actor.h"
#include "SceneView.h"

class UCameraComponent;
class UCameraModifierBase;
class FViewport;
class URenderSettings;
class UCamMod_Fade;

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	GENERATED_REFLECTION_BODY()

public:
	APlayerCameraManager() { Name = "Player Camera Manager";  };

	void StartCameraShake(float InDuration, float AmpLoc, float AmpRotDeg, float Frequency, int32 InPriority = 0);
	
	void StartFade(float InDuration, float FromAlpha, float ToAlpha, const FLinearColor& InColor= FLinearColor::Zero(), int32 InPriority = 0);
	
	void StartLetterBox(float InDuration, float Aspect, float BarHeight, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	
	int StartVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	int UpdateVignette(int Idx, float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	void AdjustVignette(float InDuration, float Radius, float Softness, float Intensity, float Roundness, const FLinearColor& InColor = FLinearColor::Zero(), int32 InPriority = 0);
	void DeleteVignette();
	
	void StartGamma(float Gamma); 

protected:
	~APlayerCameraManager() override;

	void AddModifier(UCameraModifierBase* Modifier)
	{
		ActiveModifiers.Add(Modifier);
	}

	void BuildForFrame(float DeltaTime);

public:
	TArray<UCameraModifierBase*> ActiveModifiers;

	inline void FadeIn(float Duration, const FLinearColor& Color=FLinearColor::Zero(), int32 Priority=0)
	{   // 검은 화면(1) → 씬(0)
		StartFade(Duration, 1.f, 0.f, Color, Priority);
	}
	inline void FadeOut(float Duration, const FLinearColor& Color = FLinearColor::Zero(), int32 Priority = 0)
	{   // 씬(0) → 검은 화면(1)
		StartFade(Duration, 0.f, 1.f, Color, Priority);
	}

public:
	void Destroy() override;
	// Actor의 메인 틱 함수
	void Tick(float DeltaTime) override;

	void CacheViewport(FViewport* InViewport) { CachedViewport = InViewport; }
	FMinimalViewInfo* GetSceneView();

	UCameraComponent* GetViewCamera();
	void SetViewCamera(UCameraComponent* NewViewTarget);
	void SetViewCameraWithBlend(UCameraComponent* NewViewTarget, float InBlendTime);

	DECLARE_DUPLICATE(APlayerCameraManager)

	TArray<FPostProcessModifier> GetModifiers() { return Modifiers; };

	void UpdateViewTarget(float DeltaTime);

private:
	//std::weak_ptr<UCameraComponent> CurrentViewCamera;
	UCameraComponent* CurrentViewCamera;

	FMinimalViewInfo CurrentViewInfo{};
	FMinimalViewInfo BlendStartViewInfo{};

	TArray<FPostProcessModifier> Modifiers;

	FViewport* CachedViewport = nullptr;

	float BlendTimeTotal;
	float BlendTimeRemaining;

	float TransitionCurve[4] = { 0.47f, 0.0f, 0.745f, 0.715f };

	// TODO : 감싸기 or 배열로 관리, 현재 vignette 1개만 Update 가능
	// Vignette 연속 효과를 위한 IDX
	int LastVignetteIdx = 0;
};
