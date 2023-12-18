#version 450
#pragma shader_stage(fragment)

#include "pbr_uniforms.glsl"

layout (location = 0)out vec4 o_color;

layout (location = 0)in vec3 v_position;
#if HAS_NORMAL
    layout (location = 1)in vec3 v_normal;
#endif
#if HAS_TANGENT
    layout (location = 2)in vec3 v_tangent;
#endif
#if HAS_TEXCOORD_0
    layout (location = 3)in vec2 v_texCoord_0;
#endif
#if HAS_VERTCOLOR_0
    layout (location = 4)in vec4 v_color_0;
#endif

vec3 calcDirLight(DirLight dirLight, vec3 albedo, vec3 normal, float metallic, float roughness)
{
    vec3 dir = dirLight.dir.xyz;
    vec3 color = dirLight.color.xyz;
    float cosFactor = dot(-dir, normal);
    return cosFactor * albedo * color;
}

void main()
{
    vec4 albedo = u_color;
    #if HAS_VERTCOLOR_0
        albedo *= v_color_0;
    #endif
    #if HAS_TEXCOORD_0 && HAS_COLOR_TEX
        albedo *= texture(u_colorTex, v_texCoord_0);
    #endif

    #if HAS_NORMAL
        vec3 normal = v_normal;
        #if HAS_TEXCOORD_0 && HAS_NORMAL_TEX && HAS_TANGENT
            mat3 TBN = mat3(v_normal, v_tangent, cross(v_tangent, v_normal));
            vec3 normalFromTex = texture(u_normalTex, v_texCoord_0).rgb;
            vec3 normal = TBN * normalFromTex;
        #endif
    #endif

    float metallic = u_metallicFactor;
    float roughness = u_roughnessFactor;
    #if HAS_TEXCOORD_0 && HAS_METALLIC_RUGHNESS_TEX
        vec2 metallic_roughness_fromTex = texture(u_metallicRoughnessTex, v_texCoord_0).rg;
        metallic *= metallic_roughness_fromTex[0];
        roughness *= metallic_roughness_fromTex[1];
    #endif

    vec3 color = albedo.rgb * u_ambientLight;
    #if HAS_NORMAL
        for(uint lightI = 0; lightI < u_numDirLights; lightI++) {
            DirLight dirLight = u_dirLights[lightI];
            color += calcDirLight(dirLight, albedo.rgb, normal, metallic, roughness);
        }
    #endif
    o_color = vec4(color, albedo.a);
}