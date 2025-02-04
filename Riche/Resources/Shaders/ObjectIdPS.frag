#version 460

#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : require

#include "CommonData.glsl"

layout(location = 0) flat in int inIndex;

layout(location = 0) out vec4 outFragColor;

void main() {
	outFragColor = vec4(inIndex, 0.0, 0.0, 1.0);
}