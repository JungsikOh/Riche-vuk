#version 460

#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_image_load_formatted : require

#include "CommonData.glsl"

layout(location = 0) in vec4 inPositionWS;
layout(location = 1) in vec3 inNormalWS;
layout(location = 2) in vec2 inFragTexcoord;
layout(location = 3) flat in int inIndex;


layout(set = 2, binding = 0) uniform sampler linearWrapSS;
layout(set = 2, binding = 1) uniform sampler linearClampSS;
layout(set = 2, binding = 2) uniform sampler linearBorderSS;
layout(set = 2, binding = 3) uniform sampler pointWrapSS;
layout(set = 2, binding = 4) uniform sampler pointClampSS;

layout(set = 3, binding = 0) buffer readonly SSBO_TextureID
{
	ObjectID handle[];													// SSBO
}ssbo_TextureID;
layout(set = 4, binding = 0) uniform texture2D u_DiffuseTextureList[];	// Bindless Textures
layout(set = 5, binding = 0) uniform texture2D u_ShadowTexture;


layout(location = 0) out vec4  outColour;	// Final output colour (must also have location)

void main() {
	int textureIdx = nonuniformEXT(ssbo_TextureID.handle[inIndex].materialID);
	vec4 newColor = textureLod(sampler2D(nonuniformEXT(u_DiffuseTextureList[textureIdx]), linearWrapSS), inFragTexcoord, 0);
	vec4 shadow = textureLod(sampler2D(u_ShadowTexture, linearClampSS), inFragTexcoord, 0);

	outColour = newColor;
//	outColour.xyz *= (1.0 - shadow.r);
}