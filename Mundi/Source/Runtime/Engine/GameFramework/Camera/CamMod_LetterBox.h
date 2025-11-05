#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"
#include <math.h>

class UCamMod_LetterBox : public UCameraModifierBase
{

public:
    DECLARE_CLASS(UCamMod_LetterBox, UCameraModifierBase)
    UCamMod_LetterBox() = default;
    virtual ~UCamMod_LetterBox() = default;

    FLinearColor BoxColor = FLinearColor::Zero();
    float AspectRatio = 1.f;
    float HeightBarSize = 0.0f;
    float ElapsedTime = 0.f;   // 진행 시간(초)

private:
    float CurrentAlpha = 0.f; // 이번 프레임 출력 알파
    bool  bInitialized = false;
    FViewportRect BaseRectangle;
    float         StartAspectRatio = 0.f;

public:
    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override
    {
        if (!bEnabled || !ViewInfo) return;

        // 1) 시간 진행 + 초기화
        ElapsedTime += DeltaTime;
        if (!bInitialized)
        {
            BaseRectangle = ViewInfo->ViewRect;
            StartAspectRatio = float(BaseRectangle.Width()) / float(FMath::Max(1u, BaseRectangle.Height()));
            ElapsedTime = 0.f;
            bInitialized = true;
        }

        // 2) 목표 AspectRatio로 보간
        const float T = (Duration <= 0.f) ? 1.f : FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f);
        const float CurrentAspectRatio = FMath::Lerp(StartAspectRatio, AspectRatio, T);

        // 3) BaseRectangle 기준으로 새 ViewRect 계산
        const float Width = float(BaseRectangle.Width());
        const float Height = float(BaseRectangle.Height());
        float Top = 0.f, Bottom = 0.f, Left = 0.f, Right = 0.f;

        if (HeightBarSize > 0.f)
        {
            const float HeightBar = FMath::Clamp(HeightBarSize, 0.f, 0.5f) * Height; // 각 상/하
            Top = Bottom = HeightBar;
            const float ContentHeight = FMath::Max(0.f, Height - (Top + Bottom));
            const float DesiredWidth = ContentHeight * CurrentAspectRatio;
            Left = Right = FMath::Max(0.f, 0.5f * (Width - DesiredWidth));
        }
        else
        {
            const float ViewAspectRatio = Width / Height;
            if (ViewAspectRatio > CurrentAspectRatio)
            {   // 상/하 바
                const float ContentHeight = Width / CurrentAspectRatio;
                const float Bars = FMath::Max(0.f, Height - ContentHeight);
                Top = Bottom = 0.5f * Bars;
            }
            else if (ViewAspectRatio < CurrentAspectRatio)
            {   // 좌/우 바
                const float ContentWidth = Height * CurrentAspectRatio;
                const float Bars = FMath::Max(0.f, Width - ContentWidth);
                Left = Right = 0.5f * Bars;
            }
        }

        FViewportRect Rectangle;
        Rectangle.MinX = BaseRectangle.MinX + (uint32)Left;
        Rectangle.MaxX = BaseRectangle.MaxX - (uint32)Right;
        Rectangle.MinY = BaseRectangle.MinY + (uint32)Top;
        Rectangle.MaxY = BaseRectangle.MaxY - (uint32)Bottom;
        ViewInfo->ViewRect = Rectangle; // 끝. (행렬은 기존 로직이 처리)
    };
};