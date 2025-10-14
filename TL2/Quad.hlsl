Texture2D g_PrePathResultTex : register(t0);
Texture2D g_DepthTex : register(t1);

SamplerState g_Sample : register(s0);

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    int RenderMode; // 0: fog, 1: depth
    float Near;
    float Far;
    float Padding;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 InvViewMatrix;
    row_major float4x4 InvProjectionMatrix;
}

cbuffer FogCB : register(b2)
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float4 FogInscatteringColor;
}

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = g_PrePathResultTex.Sample(g_Sample, input.texCoord);
    float depth = g_DepthTex.Sample(g_Sample, input.texCoord).r;

    float zView = Near * Far / (Far - depth * (Far - Near)); // zView값에 perspective projection -> w값(=zWorld)나누기 한 것의 역연산 = zViewW
    float NormalizedDepth = saturate((zView - Near) / (Far - Near));

    // full screen quad의 texcoord로부터 ndc좌표계의 x,y 구하기
    float ndcX = input.texCoord.x * 2.0f - 1.0f;
    float ndcY = 1.0f - input.texCoord.y * 2.0f; // y축 뒤집기
    
    float4 ndcPos = float4(ndcX, ndcY, depth, 1.0f);
    
    // 해당 픽셀의 월드좌표 구하기
    float4 worldPos = mul(mul(ndcPos, InvProjectionMatrix), InvViewMatrix);
    worldPos /= worldPos.w; // 원래 w값으로 나누기
    
    // camera의 월드좌표 구하기
    float4 cameraWorldPos = mul(float4(0, 0, 0, 1), InvViewMatrix);

    float4 camToPixel = worldPos - cameraWorldPos;
    
    if (RenderMode == 0) // exponential height fog
    {
        float L_fog = clamp(camToPixel.z - StartDistance, 0.0, FogCutoffDistance - StartDistance);

        // 높이 감쇠 포함 적분 근사
        // f = GlobalDensity * exp(-HeightFalloff * z) 함수에 대한 적분 근사
        float h = FogHeightFalloff;
        float Integral;
        if (abs(h) > 1e-6) // 안정적인 경우
        {
            Integral = (1 - exp(-h * L_fog)) / h;
        }
        else // h가 0에 가까울 때 테일러 근사
        {
            Integral = log(2.0) - (0.5 * log(2.0) * log(2.0)) * h; // 2차항까지 근사
        }
        
        float FogAmount = FogDensity * Integral;
        
        // 감쇠율
        float FogFactor = exp(-FogAmount);

        // 최대 불투명도 적용
        FogFactor = clamp(FogFactor, 1 - FogMaxOpacity, 1.0);

        // 최종 색상
        color.rgb = lerp(FogInscatteringColor.rgb, color.rgb, FogFactor);


    }
    else if(RenderMode == 1) // depth
    {
        color = float4(NormalizedDepth, NormalizedDepth, NormalizedDepth, 1.0f);
    }

    return color;
}