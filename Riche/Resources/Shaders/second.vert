#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_EXT_debug_printf : enable

// Array for triangle that fills the screen
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0)
);

layout(location = 0) out vec2 fragTex;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTex = gl_Position.xy * 0.5 + 0.5;
}