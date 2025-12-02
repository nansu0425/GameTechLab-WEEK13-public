// DoF Phase 2 - 전용 풀스크린 버텍스 쉐이더
// FViewportConstants (b10)를 사용하지 않고 단순 0~1 UV 출력
// 픽셀 쉐이더에서 RemapToSourceUV/RemapToHalfResUV로 명시적 UV 변환

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Out;

    // NDC 좌표 (-1~1) → 전체 화면 삼각형 (6 vertices = 2 triangles)
    const float2 Positions[6] =
    {
        float2(-1, 1), float2(1, 1), float2(-1, -1),   // 첫 번째 삼각형
        float2(-1, -1), float2(1, 1), float2(1, -1)   // 두 번째 삼각형
    };

    // UV 좌표 (0~1) - 렌더 타겟 전체 범위
    const float2 UVs[6] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1),     // 첫 번째 삼각형
        float2(0, 1), float2(1, 0), float2(1, 1)     // 두 번째 삼각형
    };

    Out.Position = float4(Positions[VertexID], 0.0f, 1.0f);
    Out.TexCoord = UVs[VertexID];

    return Out;
}
