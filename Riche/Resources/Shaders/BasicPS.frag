#version 460
#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : require

#include "CommonData.glsl"

layout(location = 0) in inMeshInput {
    vec4 inPos;
	vec3 inColor;
	vec4 inNormal;
    vec2 inTexcoord;
};

layout(set = 0, binding = 0) readonly uniform U_Camera
{
	mat4 view;
	mat4 projection;
    mat4 viewInverse;
    mat4 projInverse;
	
	mat4 prevView;
	mat4 prevProjection;
	mat4 prevViewInverse;
	mat4 prevProjInverse;

    vec4 camPos;
}u_Camera;

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
		vec3 cameraPos = u_Camera.camPos.xyz;
        vec3 normal = normalize(inNormal.xyz);
        vec3 fragPos = inPos.xyz;
        vec3 lightDir = normalize(u_ShaderSetting.lightPos.xyz - fragPos);
        vec3 viewDir = normalize(cameraPos - fragPos);
        vec3 reflectDir = reflect(-lightDir, normal);

        int textureIdx = nonuniformEXT(ssbo_TextureID.handle[u_ShaderSetting.batchIdx].materialID);
        vec4 newColor = textureLod(sampler2D(nonuniformEXT(u_DiffuseTextureList[textureIdx]), linearWrapSS), inTexcoord, 0);

        // Ambient
        vec3 ambient = u_ShaderSetting.ambientStrength * newColor.xyz;

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * u_ShaderSetting.lightColor.xyz * newColor.xyz;

        // Specular
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_ShaderSetting.shininess);
        vec3 specular = spec * u_ShaderSetting.specularStrength * u_ShaderSetting.lightColor.xyz;

        vec3 result = ambient + diffuse + specular;
        outColor = vec4(result, newColor.a);
	}
	else
	{
        outColor = vec4(inColor, 1.0);
	}
}