#pragma once
constexpr uint32 NUM_POINT_LIGHT_MAX = 16;
constexpr uint32 NUM_SPOT_LIGHT_MAX = 16;

struct FAmbientLightInfo
{
    FLinearColor Color;

    float Intensity;
    FVector Padding;
};

struct FDirectionalLightInfo
{
    FLinearColor Color;

    FVector Direction;
    float Intensity;
};

struct FPointLightInfo
{
    FLinearColor Color;

    FVector Position;
    float AttenuationRadius;

    FVector Attenuation;    // 상수, 일차항, 이차항
    float FalloffExponent;

    float Intensity;
    FVector Padding;
};

struct FSpotLightInfo
{
    FLinearColor Color;

    FVector Position;
    float InnerConeAngle;

    FVector Direction;
    float OuterConeAngle;
    
    FVector Attenuation;
    float AttenuationRadius;

    float Intensity;
    FVector Padding;
};