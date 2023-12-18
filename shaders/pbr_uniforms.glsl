#extension GL_EXT_scalar_block_layout : require

#define DESCSET_GLOBAL 0
#define DESCSET_MATERIAL 1
#if 0
    #define DESCSET_OBJECT 2
#endif

struct DirLight {
    vec4 dir;
    vec4 color; 
};

layout(std430, set = DESCSET_GLOBAL, binding = 0) uniform DirLights {
    //float u_ambientLight_x, u_ambientLight_y, u_ambientLight_z;
    vec3 u_ambientLight;
    uint u_numDirLights;
    DirLight u_dirLights[MAX_DIR_LIGHTS];
};

layout(set = DESCSET_MATERIAL, binding = 0) uniform MaterialUniforms {
    vec4 u_color;
    float u_metallicFactor;
    float u_roughnessFactor;
};

#if HAS_COLOR_TEX
    layout(set = DESCSET_MATERIAL, binding = 1) uniform sampler2D u_colorTex;
#endif
#if HAS_NORMAL_TEX
    layout(set = DESCSET_MATERIAL, binding = 2) uniform sampler2D u_normalTex;
#endif
#if HAS_METALLIC_RUGHNESS_TEX
    layout(set = DESCSET_MATERIAL, binding = 3) uniform sampler2D u_metallicRoughnessTex;
#endif

#if 0
    layout(set = DESCSET_OBJECT, binding = 0) uniform ObjectUniforms {
        mat4 u_model;
        mat4 u_modelViewProj;
        mat3 u_invTransModelView;
    };
#endif