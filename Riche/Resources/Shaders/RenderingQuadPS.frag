#version 460
#extension GL_ARB_shading_language_include : enable
#extension GL_ARB_shader_draw_parameters : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform sampler linearWrapSS;
layout(set = 0, binding = 1) uniform sampler linearClampSS;
layout(set = 0, binding = 2) uniform sampler linearBorderSS;
layout(set = 0, binding = 3) uniform sampler pointWrapSS;
layout(set = 0, binding = 4) uniform sampler pointClampSS;

layout(set = 1, binding = 0) uniform texture2D inputColour;
layout(set = 2, binding = 0) uniform texture2D u_ShadowTexture;

layout(location = 0) in vec2 inFragTexcoord;

layout(location = 0) out vec4 outColour;

// 3x3 �ڽ� ���� �����Ͽ� shadow �ؽ�ó�� ���� ��ճ��� �Լ�
float blurShadow9x9(texture2D shadowTex, vec2 uv)
{
    // �ؽ�ó�� �� �ȼ� ũ�� (�� �� ����)
    vec2 texelSize = 1.0 / vec2(textureSize(shadowTex, 0));
    float sum = 0.0;
    
    // -1 ~ 1 ������ �������� ����Ͽ� 9�� �ȼ� ���ø�
    for (int i = -3; i <= 3; i++)
    {
        for (int j = -3; j <= 3; j++)
        {
            vec2 offset = vec2(float(i), float(j)) * texelSize;
            // textureLod ������� LOD 0���� ���ø� (�Ǵ� texture()�� ����ص� ����)
            sum += textureLod(sampler2D(shadowTex, linearWrapSS), uv + offset, 0).r;
        }
    }
    return sum / 36.0; // 9�� ������ ���
}

void main()
{
    vec4 color = textureLod(sampler2D(inputColour, linearWrapSS), inFragTexcoord.xy, 0).rgba;

    vec4 finalColor = vec4(color.rgb, 1.0);
    finalColor.rgb *= blurShadow9x9(u_ShadowTexture, inFragTexcoord.xy);
    outColour = finalColor;
}