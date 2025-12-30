// Colored triangle shader with MVP transform and material support

cbuffer TransformBuffer : register(b0)
{
    float4x4 mvp;
};

cbuffer MaterialBuffer : register(b1)
{
    float4 base_color;
    float metallic;
    float roughness;
    float emission;
    float _pad;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Multiply vertex color by material base color
    float4 color = input.color * base_color;

    // Add emission
    color.rgb += emission;

    return color;
}
