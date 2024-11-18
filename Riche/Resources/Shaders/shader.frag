#version 460
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;

layout(location = 0) out vec4  outColour;	// Final output colour (must also have location)

void main() {
	outColour = vec4(0.5, 0.8, 0, 1);
	outColour = vec4(fragCol, 1.0);
}