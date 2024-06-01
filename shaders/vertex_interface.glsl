#pragma once

// https://github.com/KhronosGroup/glslang/blob/44fcbccd06b6e3596925d787774afe2a28d3fe16/glslang/MachineIndependent/Versions.cpp#L671
#if GL_VERTEX_SHADER

    // --- ATTRIBUTES ---
    // per-instance
    #if USE_MODEL_MTX
        layout(location = 0)in mat4 u_model;
    #endif
    layout(location = 4)in mat4 u_modelViewProj;
    #if USE_INV_TRANS_MODEL_MTX
        layout(location = 8)in mat3 u_invTransModel;
    #endif

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

    // --- VARYINGS ---
    // redefine the default gl_PerVertex: https://www.khronos.org/opengl/wiki/Built-in_Variable_(GLSL)#Vertex_shader_outputs
    out gl_PerVertex
    {
        vec4 gl_Position;
    };
    #if USES_POSITION_VARYING
        layout (location = 0)out vec3 v_position;
    #endif
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

#elif GL_FRAGMENT_SHADER

    #if USES_POSITION_VARYING
        layout (location = 0)in vec3 v_position;
    #endif
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

#else
    #error Invalid shader stage
#endif