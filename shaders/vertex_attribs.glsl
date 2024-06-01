#pragma once

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