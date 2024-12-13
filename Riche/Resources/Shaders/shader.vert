#version 460	// Use GLSL 4.5
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_debug_printf : enable

#define MAX_OBJECTS 1000

layout(location = 0) in vec3 pos; // output colour for vertex (location is required)
layout(location = 1) in vec3 col;
layout(location = 2) in vec2 tex;

layout(set = 0, binding = 0) uniform UboViewProjection {
	mat4 view;
	mat4 projection;
} uboViewProjection;

// NOT IN USE, LEFT FOR REFERENCE
layout(set = 1, binding = 0) uniform UboModel {
	mat4 model[MAX_OBJECTS];
} uboModel;

//layout(push_constant) uniform PushModel {
//	mat4 model;
//} pushModel;

layout(location = 0) out vec3 fragCol;
layout(location = 1) out vec2 fragTex;

void main() {
	mat4 model = uboModel.model[gl_BaseInstanceARB];
	debugPrintfEXT("gl_BaseInstance: %d\n", gl_BaseInstanceARB);
	gl_Position = uboViewProjection.projection * uboViewProjection.view * model * vec4(pos, 1.0);

	fragCol = col;
	fragTex = tex;
}