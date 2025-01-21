#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0) uniform sampler linearWrapSS;
layout(set = 0, binding = 1) uniform sampler linearClampSS;
layout(set = 0, binding = 2) uniform sampler linearBorderSS;
layout(set = 0, binding = 3) uniform sampler pointWrapSS;
layout(set = 0, binding = 4) uniform sampler pointClampSS;

layout(set = 1, binding = 0) uniform texture2D inputColour;

layout(location = 0) in vec2 fragTex;

layout(location = 0) out vec4 colour;

void main()
{
    vec4 color = textureLod(sampler2D(inputColour, linearWrapSS), fragTex.xy, 0).rgba;                      // 텍스처 샘플링
    vec4 finalColor = vec4(color.rgb, 1.0);
    colour = finalColor;                                             // 출력
}