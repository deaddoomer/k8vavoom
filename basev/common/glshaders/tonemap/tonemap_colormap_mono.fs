#version 120

// CM_Mono
#define CALCTMAP  vec3(min(1.0, i*1.5), min(1.0, i*1.5), i)

$include "tonemap/tonemap_colormap_common.fs"
