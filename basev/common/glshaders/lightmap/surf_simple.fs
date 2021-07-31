#version 120
$include "common/common.inc"

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
#ifdef VV_SIMPLE_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
uniform vec4 Light;

$include "common/fog_vars.fs"
$include "common/texture_vars.fs"

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
$include "common/doom_lighting.fs"
//#endif


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
#ifdef VV_SIMPLE_MASKED
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
#ifdef VV_SIMPLE_BRIGHTMAP
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  vec4 lt = calcGlowLLev(Light);
#ifdef VV_SIMPLE_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif

  TexColor.rgb *= lt.rgb;

  // convert to premultiplied
  vec4 FinalColor;
  FinalColor.a = lt.a;//TexColor.a*lt.a; //k8: non-additive and non-translucent should not end here anyway
  FinalColor.rgb = clamp(TexColor.rgb*FinalColor.a, 0.0, 1.0);
  //vec4 FinalColor = TexColor;
  $include "common/fog_calc.fs"

  out_FragValue0 = FinalColor;
}
