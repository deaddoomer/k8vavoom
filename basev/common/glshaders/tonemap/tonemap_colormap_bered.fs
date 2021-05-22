#version 120

// CM_BeRed
// "berserk red"
//#define CALCTMAP  vec3(i, i/8.0, i/8.0)
#define CALCTMAP  vec3(i, i*0.125, i*0.125)

$include "tonemap/tonemap_colormap_common.fs"
