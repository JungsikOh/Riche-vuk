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

void main()
{
    vec4 shadow = textureLod(sampler2D(u_ShadowTexture, linearClampSS), inFragTexcoord.xy, 0);
    vec4 color = textureLod(sampler2D(inputColour, linearWrapSS), inFragTexcoord.xy, 0).rgba;

    vec4 finalColor = vec4(color.rgb, 1.0);
    finalColor.rgb *= shadow.r;
    outColour = finalColor;
}