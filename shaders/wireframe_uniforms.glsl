#pragma once

#include "desc_sets.glsl"

layout(set = DESCSET_MATERIAL, binding = 0) uniform WireframeUniforms {
    vec4 u_color;
};