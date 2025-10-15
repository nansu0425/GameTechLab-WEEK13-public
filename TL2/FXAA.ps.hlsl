//--------------------------------------------------------------------------------------
// 상수 버퍼 (Constant Buffer)
// CPU에서 GPU로 넘겨주는 데이터
//--------------------------------------------------------------------------------------
cbuffer FXAAParametersCB : register(b0)
{
    float2 ScreenSize; // 화면 해상도 (e.g., float2(1920.0f, 1080.0f))
    float2 InvScreenSize; // 1.0f / ScreenSize (픽셀 하나의 크기)
    float SubpixelBlend; // 서브픽셀 블렌딩 강도 (0.75f 권장)
    float EdgeThresholdMin; // 엣지 감지 최소 휘도 차이 (0.0833f 권장)
    float EdgeThresholdMax; // 엣지 감지 최대 휘도 차이 (0.166f 권장)
};

//--------------------------------------------------------------------------------------
// 텍스처와 샘플러
// 씬 렌더링 결과가 담긴 텍스처와 이를 샘플링할 샘플러
//--------------------------------------------------------------------------------------
Texture2D g_SceneTexture : register(t0);
SamplerState g_SamplerLinear : register(s0); // 선형 필터링 샘플러 (Linear Filtering)

//--------------------------------------------------------------------------------------
// RGB 색상에서 휘도(Luminance)를 계산하는 함수
// (sRGB -> Linear 변환 후 계산하는 것이 더 정확하지만, 여기서는 단순화)
//--------------------------------------------------------------------------------------
float RGBToLuminance(float3 InColor)
{
    // ITU-R BT.709 표준에 따른 휘도 공식 (가장 일반적)
    return dot(InColor, float3(0.2126, 0.7152, 0.0722));
}

//--------------------------------------------------------------------------------------
// FXAA 픽셀 셰이더
//--------------------------------------------------------------------------------------
float4 mainPS(float4 Pos : SV_Position, float2 TexCoord : TEXCOORD0) : SV_Target
{
    float2 ScreenSize = float2(1920.0f, 1080.0f);
    float2 InvScreenSize = float2(0.00052f, 0.000926f);
    float SubpixelBlend = 0.75f;
    float EdgeThresholdMin = 0.0833f;
    float EdgeThresholdMax = 0.166f;
    
    // 1. 현재 픽셀의 색상과 휘도 가져오기
    float4 currentPixelColor = g_SceneTexture.Sample(g_SamplerLinear, TexCoord);
    float currentLuminance = RGBToLuminance(currentPixelColor.rgb);

    // 2. 주변 픽셀 샘플링 및 최대/최소 휘도 찾기
    //    중앙 픽셀과 주변 8개 픽셀 (3x3 커널)을 사용하여 휘도 차이 계산
    //    최적화를 위해 보통 +X, -X, +Y, -Y 방향만 샘플링
    
    // 이웃 픽셀들의 휘도 샘플링
    // 이웃 픽셀은 InvScreenSize (픽셀 하나의 크기) 만큼 떨어진 곳
    float l_NW = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(-1, -1)).rgb);
    float l_NE = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(1, -1)).rgb);
    float l_SW = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(-1, 1)).rgb);
    float l_SE = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(1, 1)).rgb);
    
    float l_N = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(0, -1)).rgb);
    float l_S = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(0, 1)).rgb);
    float l_W = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(-1, 0)).rgb);
    float l_E = RGBToLuminance(g_SceneTexture.Sample(g_SamplerLinear, TexCoord + InvScreenSize * float2(1, 0)).rgb);

    // 주변 픽셀들 중 최대/최소 휘도 찾기
    float maxLuminance = max(currentLuminance, max(l_N, max(l_S, max(l_W, l_E))));
    float minLuminance = min(currentLuminance, min(l_N, min(l_S, min(l_W, l_E))));

    // 3. 엣지 감지 및 블렌딩 여부 결정
    //   - 현재 픽셀이 엣지에 속하는지 판단
    //   - 엣지의 강도를 계산하여 블렌딩할지, 얼마나 블렌딩할지 결정
    
    // 현재 픽셀이 주변 픽셀들과 휘도 차이가 크지 않다면 엣지가 아님 -> 원본 색상 반환
    if ((maxLuminance - minLuminance) < EdgeThresholdMin)
    {
        return currentPixelColor;
    }

    // 엣지 강도 및 방향 계산
    float range = maxLuminance - minLuminance;
    // 엣지 강도가 너무 크면 (텍스처의 노이즈 등으로 판단) 원본 유지
    if (range < EdgeThresholdMin || range > EdgeThresholdMax)
    {
        return currentPixelColor;
    }

    // 엣지 방향 판단 (수평/수직)
    // 수평 휘도 차이와 수직 휘도 차이 중 더 큰 쪽이 엣지의 주요 방향
    float horizontalLuminanceDelta = abs(l_N + l_S - 2 * currentLuminance);
    float verticalLuminanceDelta = abs(l_W + l_E - 2 * currentLuminance);
    
    bool isHorizontal = (horizontalLuminanceDelta >= verticalLuminanceDelta);

    // 엣지 방향에 따라 탐색할 오프셋 결정
    float2 offset1, offset2;
    if (isHorizontal)
    {
        offset1 = float2(InvScreenSize.x, 0); // 수평 엣지일 경우 X축으로 탐색
        offset2 = float2(-InvScreenSize.x, 0);
    }
    else
    {
        offset1 = float2(0, InvScreenSize.y); // 수직 엣지일 경우 Y축으로 탐색
        offset2 = float2(0, -InvScreenSize.y);
    }
    
    // 4. 엣지 따라 탐색하여 새로운 색상 결정
    //    엣지 방향으로 얼마나 멀리 블렌딩할 픽셀을 찾을지 결정 (보통 2~3 픽셀 정도)
    float totalLuminance = currentLuminance;
    float4 finalColor = currentPixelColor;
    float totalWeight = 1.0f;
    
    // 엣지를 따라 양쪽으로 탐색
    for (int i = 0; i < 4; ++i) // 탐색 거리 (iterations)
    {
        float2 texcoord1 = TexCoord + offset1 * (i + 1);
        float2 texcoord2 = TexCoord + offset2 * (i + 1);
        
        float4 color1 = g_SceneTexture.Sample(g_SamplerLinear, texcoord1);
        float4 color2 = g_SceneTexture.Sample(g_SamplerLinear, texcoord2);
        
        float lum1 = RGBToLuminance(color1.rgb);
        float lum2 = RGBToLuminance(color2.rgb);

        // 엣지 바깥으로 벗어났는지 확인
        if (abs(lum1 - currentLuminance) > range * 0.5f ||
            abs(lum2 - currentLuminance) > range * 0.5f)
        {
            break; // 엣지를 벗어났다면 탐색 중지
        }
        
        // 탐색된 색상과 휘도를 누적
        totalLuminance += lum1 + lum2;
        finalColor += color1 + color2;
        totalWeight += 2.0f;
    }
    
    // 누적된 색상과 휘도의 평균 계산
    finalColor /= totalWeight;
    float avgLuminance = totalLuminance / totalWeight;

    // 최종 블렌딩 비율 결정 (현재 픽셀의 휘도와 주변 평균 휘도 기반)
    // 서브픽셀 블렌딩을 통해 작은 계단 현상을 더 부드럽게 처리
    float blendFactor = abs(avgLuminance - currentLuminance) / range;
    blendFactor = saturate(blendFactor * SubpixelBlend); // 서브픽셀 강도 조절
    
    // 최종 색상 반환: 원본 픽셀 색상과 블렌딩된 색상을 섞어줌
    return lerp(currentPixelColor, finalColor, blendFactor);
}