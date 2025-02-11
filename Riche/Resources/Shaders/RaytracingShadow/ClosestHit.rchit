#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : enable

#include "../CommonData.glsl"

struct RayBasicVertex {
    vec4 pos;
    vec4 normal;
    vec2 tex;
    vec2 padd;
};

struct Vertex {
    vec4 pos;
    vec4 normal;
    vec2 tex;
    vec2 pad;
};

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 2) rayPayloadEXT bool shadowed;
hitAttributeEXT vec2 attribs;

layout(set = 0, binding = 0) readonly uniform U_Camera
{
	mat4 view;
	mat4 projection;
    mat4 viewInverse;
    mat4 projInverse;
}u_Camera;

layout(set = 1, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = 2, scalar) buffer Vertices { RayBasicVertex v[]; } vertices;
layout(set = 1, binding = 3, scalar) buffer Indices { uint i[]; } indices;


void main()
{
    ivec3 index = ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1], indices.i[3 * gl_PrimitiveID + 2]);

    RayBasicVertex v0 = vertices.v[index.x];
    RayBasicVertex v1 = vertices.v[index.y];
    RayBasicVertex v2 = vertices.v[index.z];

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z);
    vec2 tex = (v0.tex * barycentricCoords.x + v1.tex * barycentricCoords.y + v2.tex * barycentricCoords.z);

    // Basic lighting
    vec3 lightVector = normalize(u_ShaderSetting.lightPos.xyz);
    float dot_product = max(dot(lightVector, normal), 0.6);
    hitValue = vec3(1, 0, 0) * dot_product;

    // Shadow Casting
    float tMin = 0.001;
    float tMax = 10000.0;
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 biasedOrigin = origin + normal * 0.001; // 자기 교차 방지용 offset
    
    shadowed = true;

    // Trace shadow ray and offset indices to match shadow hit/miss shader group indices
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, biasedOrigin, tMin, lightVector, tMax, 2);

    if(dot(normal, lightVector) < 0.0)
		shadowed = true;

    if (shadowed) {
		hitValue *= 0.3;
	}
}