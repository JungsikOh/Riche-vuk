#version 460
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_ARB_shading_language_include : enable

#include "CommonData.glsl"

layout(set = 2, binding = 0) uniform texture2D diffuseTextureList[];	// Bindless Textures

layout(set = 3, binding = 0) buffer readonly MaterialID
{
	ObjectID handle[];													// SSBO
}materialIdBuffer;

layout(set = 4, binding = 0) uniform sampler linearWrapSS;
layout(set = 4, binding = 1) uniform sampler linearClampSS;
layout(set = 4, binding = 2) uniform sampler linearBorderSS;
layout(set = 4, binding = 3) uniform sampler pointWrapSS;
layout(set = 4, binding = 4) uniform sampler pointClampSS;


layout(push_constant) uniform readonly ShaderSetting {
	uint is_debugging;
	uint batch_idx;
} shaderSetting;

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;
layout(location = 2) flat in int idx;

layout(location = 0) out vec4  outColour;	// Final output colour (must also have location)

void main() {
	int textureIdx = nonuniformEXT(materialIdBuffer.handle[idx].materialId);
	vec4 newColor = textureLod(sampler2D(nonuniformEXT(diffuseTextureList[textureIdx]), linearWrapSS), fragTex, 0);

	float alpha = (length(newColor.rgb) < 0.001) ? 0.0 : 1.0;
//	outColour = vec4(0.5, 0.8, 0, 1);
	outColour = newColor;
}