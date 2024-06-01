#version 450
#pragma shader_stage(vertex)

#include "vertex_interface.glsl"

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
