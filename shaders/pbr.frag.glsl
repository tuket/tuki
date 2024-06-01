#version 450
#pragma shader_stage(fragment)

#include "pbr_uniforms.glsl"
#include "vertex_interface.glsl"

layout (location = 0)out vec4 o_color;

vec3 calcDirLight(DirLight dirLight, vec3 albedo, vec3 normal, float metallic, float roughness)
{
    vec3 dir = dirLight.dir.xyz;
    vec3 color = dirLight.color.xyz;
    float cosFactor = max(0, dot(dir, normal));
    return cosFactor * albedo * color;
}

void main()
{
    vec4 albedo = u_color;
    #if HAS_VERTCOLOR_0
        albedo *= v_color_0;
    #endif
    #if HAS_TEXCOORD_0 && HAS_ALBEDO_TEX
        albedo *= texture(u_albedoTex, v_texCoord_0);
    #endif

    #if HAS_NORMAL
        vec3 normal = normalize(v_normal);
        #if HAS_TEXCOORD_0 && HAS_NORMAL_TEX && HAS_TANGENT
            vec3 tangent = normalize(v_tangent);
            mat3 TBN = mat3(normal, tangent, cross(tangent, normal));
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