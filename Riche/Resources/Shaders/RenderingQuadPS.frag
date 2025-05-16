#version 460
#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "CommonData.glsl"

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

const float g5[5] = float[5](
    0.06136, 0.24477, 0.38774, 0.24477, 0.06136                      // 정규화된 가중치
);

vec3 gaussianBlurLit(texture2D colTex, texture2D shTex, vec2 uv)
{
    vec2 d = 1.0 / vec2(textureSize(colTex, 0));
    vec3 acc = vec3(0.0);
    for (int i = -2; i <= 2; ++i) {
        float wi = g5[i + 2];
        for (int j = -2; j <= 2; ++j) {
            float w = wi * g5[j + 2];
            vec2  off = vec2(i, j) * d;
            vec3  c   = textureLod(sampler2D(colTex, linearWrapSS), uv + off, 0).rgb;
            float sh  = textureLod(sampler2D(shTex, linearWrapSS), uv + off, 0).r;
            acc += c * sh * w;                     // 조명 결과에 필터 적용
        }
    }
    return acc;                                    // 1.0 로 노멀라이즈됨
}

float brightness(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 blurLit(texture2D colTex, texture2D shTex, vec2 uv)
{
    vec2 d = 1.0 / vec2(textureSize(colTex, 0));
    vec3 bloom = vec3(0.0);
    for (int y = -3; y <= 3; ++y) {
           for (int x = -3; x <= 3; ++x) {
                vec2 offset = vec2(x, y) * d;

                vec3 bloomSample = textureLod(sampler2D(colTex, linearWrapSS), uv + offset, 0).rgb;
                float b = brightness(bloomSample);
                float visibility = textureLod(sampler2D(shTex, linearWrapSS), uv + offset, 0).r;

                if (b > 0.9999 && visibility > 0.9) { // Shadow condition plus
                    bloom += bloomSample;
                }
            }
    }
    bloom /= 49.0;
    bloom *= 0.2;
    return bloom;
}

/*────────── ACES Filmic Tone-Mapping ──────────*/
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 color = textureLod(sampler2D(inputColour, linearWrapSS), inFragTexcoord.xy, 0).rgba;

    float shadowFactor = blurShadow9x9(u_ShadowTexture, inFragTexcoord.xy);
    // shadowFactor = textureLod(sampler2D(u_ShadowTexture, linearWrapSS), inFragTexcoord.xy, 0).r; // Test

    vec3 lit = color.rgb;
    float luminance  = dot(color.rgb * shadowFactor, vec3(0.2126, 0.7152, 0.0722));
    float bloomMask  = smoothstep(0.8, 1.0, luminance);             // 임계값 0.8 ↔ 1.0

    vec3 blurred = gaussianBlurLit(inputColour, u_ShadowTexture, inFragTexcoord.xy);

    lit = mix(color.rgb * shadowFactor, blurred, bloomMask);

    vec3 mapped = lit;
    if(u_ShaderSetting.isTonemapping != 0) {
        mapped = ACESFilm(lit * 0.6);
        mapped = pow(mapped, vec3(1.0 / 2.2));   // sRGB γ
    } else {
        mapped = lit;
    }

    /* 6) 그림자 팩터 곱해서 최종 색 산출 */
    outColour = vec4(mapped, 1.0);
}