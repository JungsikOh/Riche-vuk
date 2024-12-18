#version 460	// Use GLSL 4.5
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_debug_printf : enable

#define MAX_OBJECTS 1000

layout(location = 0) in vec3 pos; // output colour for vertex (location is required)
layout(location = 1) in vec3 col;
layout(location = 2) in vec2 tex;

layout(set = 0, binding = 0) uniform ViewProjectionUBO {
	mat4 view;
	mat4 projection;
} viewProjectionUBO;

layout(set = 1, binding = 0) uniform ModelListUBO {
	mat4 model[MAX_OBJECTS];
} modelListUBO;

void main() {
	mat4 model = modelListUBO.model[gl_BaseInstanceARB];
	debugPrintfEXT("gl_BaseInstance: %d\n", gl_BaseInstanceARB);
	gl_Position = viewProjectionUBO.projection * viewProjectionUBO.view * model * vec4(pos, 1.0);
}