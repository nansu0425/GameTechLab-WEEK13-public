//================================================================================================
// ParticleRibbon.hlsl
//
// 리본(Ribbon) 파티클을 렌더링하기 위한 셰이더입니다.
// 텍스처를 사용하지 않고 정점 색상으로 솔리드 컬러를 출력합니다.
//================================================================================================

//================================================================================================
// Constant Buffers
//================================================================================================

// Per-View/Frame 상수 버퍼 (디버깅으로 확인된 b1 레지스터 사용)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

//================================================================================================
// Structures
//================================================================================================

// 정점 셰이더 입력 구조체 (C++의 FParticleRibbonVertex와 일치)
struct VS_INPUT
{
    float4 Position     : POSITION;
    float3 ControlPoint : CONTROLPOINT;
    float3 Tangent      : TANGENT;
    float4 Color        : COLOR;
    float2 UV           : TEXCOORD0;
};

// 픽셀 셰이더 입력 구조체 (정점 셰이더의 출력)
struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color    : COLOR0; // 정점 색상을 픽셀 셰이더로 전달
    float2 UV       : TEXCOORD0; // 텍스처 샘플링에는 사용되지 않지만 전달됨
};


//================================================================================================
// Vertex Shader
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;

    // 정점의 위치는 이미 월드 공간에 있으므로 뷰, 투영 변환만 수행합니다.
    float4 worldPosition = float4(input.Position.xyz, 1.0f);
    float4 viewPosition = mul(worldPosition, ViewMatrix);
    output.Position = mul(viewPosition, ProjectionMatrix);

    // 정점 색상과 UV 좌표를 픽셀 셰이더로 전달합니다.
    output.Color = input.Color;
    output.UV = input.UV;

    return output;
}


//================================================================================================
// Pixel Shader
//================================================================================================
float4 mainPS(PS_INPUT input) : SV_Target
{
    // 텍스처 없이 정점 색상만 사용하여 솔리드 컬러를 출력합니다.
    // C++에서 FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)로 설정되어 있을 것입니다.
    return input.Color;
}