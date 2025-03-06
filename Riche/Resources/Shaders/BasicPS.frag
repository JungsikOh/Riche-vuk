#version 460
#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : require

#include "CommonData.glsl"

layout(location = 0) in inMeshInput {
	vec3 inColor;
	vec4 inNormal;
    vec2 inTexcoord;
};

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 3) buffer readonly SSBO_TextureID
{
	ObjectID handle[];													// SSBO
}ssbo_TextureID;

layout(set = 3, binding = 0) uniform sampler linearWrapSS;
layout(set = 3, binding = 1) uniform sampler linearClampSS;
layout(set = 3, binding = 2) uniform sampler linearBorderSS;
layout(set = 3, binding = 3) uniform sampler pointWrapSS;
layout(set = 3, binding = 4) uniform sampler pointClampSS;

layout(set = 4, binding = 0) uniform texture2D u_DiffuseTextureList[];	// Bindless Textures

void main()
{
	if(u_ShaderSetting.isMeshletRender == 0) 
	{
		int textureIdx = nonuniformEXT(ssbo_TextureID.handle[u_ShaderSetting.batchIdx].materialID);
		vec4 newColor = textureLod(sampler2D(nonuniformEXT(u_DiffuseTextureList[textureIdx]), linearWrapSS), inTexcoord, 0);
		outColor = newColor;
	}
	else
	{
		outColor = vec4(inColor, 1.0);	
	}
}