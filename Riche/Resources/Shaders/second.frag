#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_debug_printf : enable

layout(set = 0, binding = 0) uniform sampler2D inputColour;
layout(set = 0, binding = 1) uniform sampler2D inputDepth;

layout(location = 0) in vec2 fragTex;

layout(location = 0) out vec4 colour;

void main()
{
    vec4 color = texture(inputColour, fragTex.xy);                      // 텍스처 샘플링
    colour = color;                                             // 출력
}