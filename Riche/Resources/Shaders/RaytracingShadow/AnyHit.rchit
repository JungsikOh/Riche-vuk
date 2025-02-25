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

struct Offset {
    uint vertexOffset;
    uint indexOffset;
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
layout(set = 1, binding = 4) buffer Offsets { Offset o[]; } offset;

layout(set = 2, binding = 3) buffer readonly SSBO_TextureID
{
	ObjectID handle[];													// SSBO
}ssbo_TextureID;

layout(set = 3, binding = 0) uniform texture2D u_DiffuseTextureList[];	// Bindless Textures

layout(set = 4, binding = 0) uniform sampler linearWrapSS;
layout(set = 4, binding = 1) uniform sampler linearClampSS;
layout(set = 4, binding = 2) uniform sampler linearBorderSS;
layout(set = 4, binding = 3) uniform sampler pointWrapSS;
layout(set = 4, binding = 4) uniform sampler pointClampSS;

void main()
{
    uint customID = gl_InstanceCustomIndexEXT; 
    uint vertexOffset = offset.o[customID].vertexOffset;
    uint indexOffset = offset.o[customID].indexOffset;

    ivec3 index = ivec3(indices.i[3 * gl_PrimitiveID + indexOffset], indices.i[3 * gl_PrimitiveID + 1+ indexOffset], indices.i[3 * gl_PrimitiveID + 2+ indexOffset]);

    RayBasicVertex v0 = vertices.v[index.x + vertexOffset];
    RayBasicVertex v1 = vertices.v[index.y + vertexOffset];
    RayBasicVertex v2 = vertices.v[index.z + vertexOffset];

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z);
    vec2 tex = (v0.tex * barycentricCoords.x + v1.tex * barycentricCoords.y + v2.tex * barycentricCoords.z);

    float alpha = texture(alphaTestTexture, /* uv */).a;

    // 예: 알파가 특정 임계값(threshold)보다 작으면 레이를 통과시킴
    if(alpha < 0.5) {
        // 아무것도 안 맞은 것으로 취급
        ignoreIntersectionEXT();
        return;
    }

}
