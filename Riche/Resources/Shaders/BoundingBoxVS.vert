#version 460

#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : require

#include "CommonData.glsl"


layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform U_Camera
{
	mat4 view;
	mat4 projection;
} u_Camera;

layout(set = 1, binding = 0) readonly buffer SSBO_Model
{
	Transform transform[];
}ssbo_Model;


void main() {
	mat4 model = nonuniformEXT(ssbo_Model.transform[u_ShaderSetting.batchIdx].currentModel);
	gl_Position = u_Camera.projection * u_Camera.view * model * vec4(inPosition, 1.0);
}