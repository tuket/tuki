#version 450
#pragma shader_stage(vertex)

#include "pbr_uniforms.glsl"

// per-instance
layout(location = 0)in mat4 u_model;
layout(location = 4)in mat4 u_modelViewProj;
layout(location = 8)in mat3 u_invTransModel; 

// per-vertex
layout(location = 11)in vec3 a_position;
#if HAS_NORMAL
    layout(location = 12)in vec3 a_normal;
#endif
#if HAS_TANGENT
    layout(location = 13)in vec3 a_tangent;
#endif
#if HAS_TEXCOORD_0
    layout(location = 14)in vec2 a_texCoord_0;
#endif
#if HAS_VERTCOLOR_0
    layout(location = 15)in vec4 a_color_0;
#endif
#if defined(ENABLE_SKINNING)
    layout(location = 16)in vec4 a_skinning_joints;
    layout(location = 17)in vec4 a_skinning_weights;
#endif

// redefine the default gl_PerVertex: https://www.khronos.org/opengl/wiki/Built-in_Variable_(GLSL)#Vertex_shader_outputs
out gl_PerVertex
{
    vec4 gl_Position;
};
layout (location = 0)out vec3 v_position;
#if HAS_NORMAL
    layout (location = 1)out vec3 v_normal;
#endif
#if HAS_TANGENT
    layout (location = 2)out vec3 v_tangent;
#endif
#if HAS_TEXCOORD_0
    layout (location = 3)out vec2 v_texCoord_0;
#endif
#if HAS_VERTCOLOR_0
    layout (location = 4)out vec4 v_color_0;
#endif

void main()
{
    v_position = (u_model * vec4(a_position, 1)).xyz;
    gl_Position = u_modelViewProj * vec4(a_position, 1);

    #if HAS_NORMAL
        v_normal = u_invTransModel * a_normal;
        #if HAS_TANGENT
            v_tangent = u_invTransModel * a_tangent;
        #endif
    #endif

    #if HAS_TEXCOORD_0
        v_texCoord_0 = a_texCoord_0;
    #endif
        
    #if HAS_VERTCOLOR_0
        v_color_0 = a_color_0;
    #endif
}
