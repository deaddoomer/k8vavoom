#version 120
$include "common/common.inc"

uniform vec4 Light; // a is light level

#ifdef VV_AMBIENT_MASKED_WALL
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
uniform sampler2D Texture;
$include "common/brightmap_vars.fs"
$include "common/texture_vars.fs"
#endif


//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
$include "common/doom_lighting.fs"
//#endif


void main () {
//#ifdef VV_AMBIENT_GLOW
  vec4 lt = calcGlowLLev(Light);
//#else
//  vec4 lt = Light;
//#endif
#ifdef VV_AMBIENT_MASKED_WALL
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  //if (TexColor.a <= ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  //if (TexColor.a <= ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
  $include "common/brightmap_calc.fs"
#endif
  out_FragValue0 = clamp(lt, 0.0, 1.0);
}
