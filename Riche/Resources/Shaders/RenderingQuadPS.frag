#version 460
#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform sampler linearWrapSS;
layout(set = 0, binding = 1) uniform sampler linearClampSS;
layout(set = 0, binding = 2) uniform sampler linearBorderSS;
layout(set = 0, binding = 3) uniform sampler pointWrapSS;
layout(set = 0, binding = 4) uniform sampler pointClampSS;

layout(set = 1, binding = 0) uniform texture2D inputColour;
layout(set = 2, binding = 0) uniform texture2D u_ShadowTexture;

layout(location = 0) in vec2 inFragTexcoord;

layout(location = 0) out vec4 outColour;

// 3x3 박스 블러를 적용하여 shadow 텍스처의 값을 평균내는 함수
float blurShadow9x9(texture2D shadowTex, vec2 uv)
{
    // 텍스처의 한 픽셀 크기 (각 축 기준)
    vec2 texelSize = 1.0 / vec2(textureSize(shadowTex, 0));
    float sum = 0.0;
    
    // -1 ~ 1 범위의 오프셋을 사용하여 9개 픽셀 샘플링
    for (int i = -3; i <= 3; i++)
    {
        for (int j = -3; j <= 3; j++)
        {
            vec2 offset = vec2(float(i), float(j)) * texelSize;
            // textureLod 사용으로 LOD 0에서 샘플링 (또는 texture()를 사용해도 무방)
            sum += textureLod(sampler2D(shadowTex, linearWrapSS), uv + offset, 0).r;
        }
    }
    return sum / 36.0; // 9개 샘플의 평균
}

void main()
{
    vec4 color = textureLod(sampler2D(inputColour, linearWrapSS), inFragTexcoord.xy, 0).rgba;

    vec4 finalColor = vec4(color.rgb, 1.0);
    finalColor.rgb *= blurShadow9x9(u_ShadowTexture, inFragTexcoord.xy);
    outColour = finalColor;
}