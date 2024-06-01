#version 450
#pragma shader_stage(vertex)

#include "vertex_interface.glsl"
#include "wireframe_uniforms.glsl"

void main()
{
    gl_Position = u_modelViewProj * vec4(a_position, 1);

    #if HAS_VERTCOLOR_0
        v_color_0 = a_color_0 * u_color;
    #endif
}
