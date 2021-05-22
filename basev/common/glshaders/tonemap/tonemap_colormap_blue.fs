#version 120

// CM_Blue
//#define CALCTMAP  vec3(i/8.0, i/8.0, i)
#define CALCTMAP  vec3(i*0.125, i*0.125, i)

$include "tonemap/tonemap_colormap_common.fs"
