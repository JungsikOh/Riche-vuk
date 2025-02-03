struct ObjectID {
    int materialID;
    float dummy[3];
};

struct Transform {
    mat4 startModel;
    mat4 currentModel;
};

// AABB 구조체
struct AABB
{
    vec4 minPos;	// minimum coord
    vec4 maxPos;	// maximum coord
};

struct BoundingSphere {
    vec3 center;
    float radius;
};

// Indirect Draw 명령
struct IndircetDrawIndexedCommand {
    uint    indexCount;
    uint    instanceCount;
    uint    firstIndex;
    int     vertexOffset;
    uint    firstInstance;
};

layout(push_constant) uniform readonly U_ShaderSetting 
{
	uint isDebugging;
	uint batchIdx;
}u_ShaderSetting;