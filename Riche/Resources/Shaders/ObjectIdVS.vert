#version 460

#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

#include "CommonData.glsl"

layout(location = 0) in vec3 inPosition; // output colour for vertex (location is required)
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

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
}u_Camera;

layout(set = 1, binding = 0) readonly buffer SSBO_Model
{
	Transform transform[];
}ssbo_Model;

layout(location = 0) out int outIndex;

void main() {
	mat4 model = nonuniformEXT(ssbo_Model.transform[u_ShaderSetting.batchIdx + gl_BaseInstanceARB].currentModel);
	gl_Position = u_Camera.projection * u_Camera.view * model * vec4(inPosition, 1.0);

	outIndex = int(u_ShaderSetting.batchIdx + gl_BaseInstanceARB);

}