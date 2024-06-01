#version 450
#pragma shader_stage(fragment)

#include "vertex_interface.glsl"
#include "wireframe_uniforms.glsl"

layout (location = 0)out vec4 o_color;

void main()
{
    #if HAS_VERTCOLOR_0
        vec4 color = v_color; // already pre-multiplied in the VS
    #else    
        vec4 color = u_color;
    #endif

    o_color = color;
}