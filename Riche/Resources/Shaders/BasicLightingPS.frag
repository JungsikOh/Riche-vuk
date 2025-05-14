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

layout(set = 1, binding = 3) buffer readonly SSBO_TextureID
{
	ObjectID handle[];													// SSBO
}ssbo_TextureID;

layout(set = 2, binding = 0) uniform sampler linearWrapSS;
layout(set = 2, binding = 1) uniform sampler linearClampSS;
layout(set = 2, binding = 2) uniform sampler linearBorderSS;
layout(set = 2, binding = 3) uniform sampler pointWrapSS;
layout(set = 2, binding = 4) uniform sampler pointClampSS;

layout(set = 3, binding = 0) uniform texture2D u_DiffuseTextureList[];	// Bindless Textures

layout(location = 0) out vec4  outColour;	// Final output colour (must also have location)

void main() {
    vec3 cameraPos = u_Camera.camPos.xyz;
    vec3 normal = normalize(inNormalWS);
    vec3 fragPos = inPositionWS.xyz;
    vec3 lightDir = normalize(u_ShaderSetting.lightPos.xyz - fragPos);
    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);

    int textureIdx = nonuniformEXT(ssbo_TextureID.handle[u_ShaderSetting.batchIdx].materialID);
    vec4 newColor = textureLod(sampler2D(nonuniformEXT(u_DiffuseTextureList[textureIdx]), linearWrapSS), inFragTexcoord, 0);

    // Ambient
    vec3 ambient = u_ShaderSetting.ambientStrength * newColor.xyz;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * u_ShaderSetting.lightColor.xyz * newColor.xyz;

    // Specular
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_ShaderSetting.shininess);
    vec3 specular = spec * u_ShaderSetting.specularStrength * u_ShaderSetting.lightColor.xyz;

    vec3 result = ambient + diffuse + specular;
    outColour = vec4(result, newColor.a);

}