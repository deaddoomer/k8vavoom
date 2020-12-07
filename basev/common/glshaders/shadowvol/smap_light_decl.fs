uniform samplerCube ShadowTexture;
varying vec3 VertWorldPos;
uniform vec3 LightPos2;
uniform float BiasMul;
uniform float BiasMax;
uniform float BiasMin;
uniform float CubeSize;
//uniform float CubeBlur;

uniform vec3 LightPos;
uniform float UseAdaptiveBias;

uniform float SurfDist;

$include "shadowvol/cubemap_conv.inc.fs"
