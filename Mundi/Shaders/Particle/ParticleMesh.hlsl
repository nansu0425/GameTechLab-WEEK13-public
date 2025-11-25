//================================================================================================
// Filename:      ParticleMesh.hlsl
// Description:   GPU 인스턴싱을 사용한 메시 파티클 렌더링
//                슬롯 0: 메시 버텍스, 슬롯 1: 인스턴스 데이터
//
// 지원 뷰 모드:
//   - Unlit (기본): 조명 계산 없이 텍스처 × 컬러 출력
//   - Lit (LIGHTING_MODEL_PHONG/LAMBERT): 픽셀별 조명 계산
//   - Gouraud: 정점별 조명 계산 (VS에서 처리)
//
// 조명 지원:
//   - Ambient Light
//   - Directional Light (섀도우 지원)
//   - Point Lights (섀도우 지원)
//   - Spot Lights (섀도우 지원, VSM 포함)
//================================================================================================

//------------------------------------------------------------------------------------------------
// Common Lighting Includes
//------------------------------------------------------------------------------------------------
#include "../Common/LightStructures.hlsl"
#include "../Common/LightingBuffers.hlsl"
#include "../Common/LightingCommon.hlsl"

//------------------------------------------------------------------------------------------------
// Constant Buffers
//------------------------------------------------------------------------------------------------

// b1: ViewProjBuffer (VS)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

//------------------------------------------------------------------------------------------------
// Input/Output Structures
//------------------------------------------------------------------------------------------------

// 슬롯 0: 메시 버텍스 (FVertexDynamic)
struct VS_MESH_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 UV : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 VertexColor : COLOR;
};

// 슬롯 1: 인스턴스 데이터 (FMeshParticleInstanceVertex)
struct VS_INSTANCE_INPUT
{
    float4 InstanceColor : COLOR1;      // 파티클 색상
    float4 Transform0 : TEXCOORD1;      // 행렬 첫번째 행
    float4 Transform1 : TEXCOORD2;      // 행렬 두번째 행
    float4 Transform2 : TEXCOORD3;      // 행렬 세번째 행
    float RelativeTime : TEXCOORD4;     // 상대 시간
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;        // 조명 계산용
    float3 WorldNormal : NORMAL0;
    float2 UV : TEXCOORD1;
    float4 Color : COLOR;
    float RelativeTime : TEXCOORD2;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
};

//------------------------------------------------------------------------------------------------
// Resources
//------------------------------------------------------------------------------------------------

// 텍스처
Texture2D MeshTexture : register(t0);

// 섀도우 맵 (LightManager에서 바인딩)
TextureCubeArray g_ShadowAtlasCube : register(t8);   // Point Light 섀도우
Texture2D g_ShadowAtlas2D : register(t9);            // Directional/Spot Light 섀도우
Texture2D<float2> g_VSMShadowAtlas : register(t10); // VSM 섀도우

// 샘플러
SamplerState LinearSampler : register(s0);
SamplerComparisonState g_ShadowSample : register(s2);
SamplerState g_VSMSampler : register(s3);

//------------------------------------------------------------------------------------------------
// Particle Lighting Helper (기존 LightingCommon 함수 활용)
//------------------------------------------------------------------------------------------------

// 메시 파티클용 전체 조명 계산 (섀도우 포함)
float3 CalculateParticleLighting(
    float3 worldPos,
    float3 viewPos,
    float3 normal,
    float3 viewDir,
    float4 baseColor,
    bool includeSpecular,
    float specularPower)
{
    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // 1. Ambient Light
    litColor += CalculateAmbientLight(AmbientLight, baseColor.rgb);

    // 2. Directional Light (기존 함수 사용 - 섀도우 포함)
    litColor += CalculateDirectionalLight(
        DirectionalLight,
        worldPos,
        viewPos,
        normal,
        viewDir,
        baseColor,
        includeSpecular,
        specularPower,
        g_ShadowAtlas2D,
        g_ShadowSample
    );

    // 3. Point Lights (기존 함수 사용 - 섀도우 포함)
    for (uint i = 0; i < PointLightCount; i++)
    {
        litColor += CalculatePointLight(
            g_PointLightList[i],
            worldPos,
            normal,
            viewDir,
            baseColor,
            includeSpecular,
            specularPower,
            g_ShadowAtlasCube,
            g_ShadowSample
        );
    }

    // 4. Spot Lights (기존 함수 사용 - 섀도우 + VSM 포함)
    for (uint j = 0; j < SpotLightCount; j++)
    {
        litColor += CalculateSpotLight(
            g_SpotLightList[j],
            worldPos,
            normal,
            viewDir,
            baseColor,
            includeSpecular,
            specularPower,
            g_ShadowAtlas2D,
            g_ShadowSample,
            g_VSMShadowAtlas,
            g_VSMSampler
        );
    }

    return litColor;
}

//------------------------------------------------------------------------------------------------
// Vertex Shader
//------------------------------------------------------------------------------------------------

PS_INPUT mainVS(VS_MESH_INPUT mesh, VS_INSTANCE_INPUT inst)
{
    PS_INPUT output;

    // 3x4 행렬에서 Transform 추출 (전치되어 저장됨)
    float4x4 InstanceTransform = float4x4(
        float4(inst.Transform0.x, inst.Transform1.x, inst.Transform2.x, 0.0f),
        float4(inst.Transform0.y, inst.Transform1.y, inst.Transform2.y, 0.0f),
        float4(inst.Transform0.z, inst.Transform1.z, inst.Transform2.z, 0.0f),
        float4(inst.Transform0.w, inst.Transform1.w, inst.Transform2.w, 1.0f)
    );

    // 월드 위치 계산
    float4 worldPos = mul(float4(mesh.Position, 1.0f), InstanceTransform);
    output.WorldPos = worldPos.xyz;

    // View-Projection 변환
    output.Position = mul(worldPos, mul(ViewMatrix, ProjectionMatrix));

    // 노멀 변환 (회전만 적용, 스케일 보정 없음 - 균등 스케일 가정)
    float3x3 RotationMatrix = (float3x3)InstanceTransform;
    output.WorldNormal = normalize(mul(mesh.Normal, RotationMatrix));

    // UV 복사
    output.UV = mesh.UV;

    // 베이스 컬러: 인스턴스 색상 × 버텍스 색상
    float4 baseColor = inst.InstanceColor * mesh.VertexColor;

#if defined(LIGHTING_MODEL_GOURAUD)
    //-----------------------------------------------------------------------------------
    // Gouraud Shading: 정점별 조명 계산 (섀도우 포함)
    //-----------------------------------------------------------------------------------
    float3 viewDir = normalize(CameraPosition - worldPos.xyz);
    float4 viewPos = mul(worldPos, ViewMatrix);
    float specularPower = 32.0f;

    float3 litColor = CalculateParticleLighting(
        worldPos.xyz,
        viewPos.xyz,
        output.WorldNormal,
        viewDir,
        baseColor,
        true,  // includeSpecular
        specularPower
    );

    output.Color = float4(litColor, baseColor.a);
#else
    // Unlit, Lambert, Phong: 베이스 컬러만 전달 (PS에서 처리)
    output.Color = baseColor;
#endif

    output.RelativeTime = inst.RelativeTime;

    return output;
}

//------------------------------------------------------------------------------------------------
// Pixel Shader
//------------------------------------------------------------------------------------------------

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;

    // 텍스처 샘플링
    float4 texColor = MeshTexture.Sample(LinearSampler, input.UV);

    // 최종 색상: 텍스처 × 정점 컬러
    output.Color = texColor * input.Color;

#if defined(LIGHTING_MODEL_PHONG)
    //-----------------------------------------------------------------------------------
    // Phong Shading: 픽셀별 조명 계산 (Specular + 섀도우 포함)
    //-----------------------------------------------------------------------------------
    float3 normal = normalize(input.WorldNormal);
    float3 viewDir = normalize(CameraPosition - input.WorldPos);
    float4 viewPos = mul(float4(input.WorldPos, 1.0f), ViewMatrix);
    float specularPower = 32.0f;

    float3 litColor = CalculateParticleLighting(
        input.WorldPos,
        viewPos.xyz,
        normal,
        viewDir,
        output.Color,
        true,  // includeSpecular
        specularPower
    );

    output.Color.rgb = litColor;

#elif defined(LIGHTING_MODEL_LAMBERT)
    //-----------------------------------------------------------------------------------
    // Lambert Shading: 픽셀별 조명 계산 (Diffuse만 + 섀도우 포함)
    //-----------------------------------------------------------------------------------
    float3 normal = normalize(input.WorldNormal);
    float3 viewDir = normalize(CameraPosition - input.WorldPos);
    float4 viewPos = mul(float4(input.WorldPos, 1.0f), ViewMatrix);

    float3 litColor = CalculateParticleLighting(
        input.WorldPos,
        viewPos.xyz,
        normal,
        viewDir,
        output.Color,
        false,  // no specular
        1.0f
    );

    output.Color.rgb = litColor;

#elif defined(LIGHTING_MODEL_GOURAUD)
    // Gouraud: VS에서 이미 조명 계산됨
    // 텍스처만 곱해주면 됨 (input.Color에 이미 조명 포함)

#endif
    // Unlit (매크로 미정의): 조명 없이 그대로 출력

    return output;
}
