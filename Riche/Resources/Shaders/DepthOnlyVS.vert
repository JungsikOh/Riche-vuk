#version 460	// Use GLSL 4.5
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

#define MAX_OBJECTS 1000

layout(location = 0) in vec3 pos; // output colour for vertex (location is required)
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex;

layout(set = 0, binding = 0) uniform ViewProjectionUBO {
	mat4 view;
	mat4 projection;
} viewProjectionUBO;

layout(set = 1, binding = 0) buffer readonly Transform {
	mat4 start_model[];
	mat4 curr_model[];
} transformUBO;

layout(push_constant) uniform readonly ShaderSetting {
	uint is_debugging;
	uint batch_idx;
} shaderSetting;

void main() {
	debugPrintfEXT("batch_idx : %d , Instance : %d \n", shaderSetting.batch_idx, gl_BaseInstanceARB);
	mat4 model = nonuniformEXT(transformUBO.curr_model[shaderSetting.batch_idx + gl_BaseInstanceARB]);
	gl_Position = viewProjectionUBO.projection * viewProjectionUBO.view * model * vec4(pos, 1.0);
}