#version 120
$include "common/common.inc"

//#define VAVOOM_ADV_MASKED_FOG

uniform sampler2D Texture;
$include "common/texshade.inc"
#ifdef VV_MASKED_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

//varying vec2 TextureCoordinate;
$include "common/texture_vars.fs"

#ifdef VV_MASKED_GLOW
$include "common/glow_vars.fs"
#endif


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  if (TexColor.a < AlphaRef) discard;
  //TexColor *= Light;

#ifdef VV_MASKED_GLOW
  vec4 lt = calcGlow(Light);
#else
  vec4 lt = Light;
#endif
#ifdef VV_MASKED_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  //TexColor *= lt;
  TexColor.rgb *= lt.rgb;

  // convert to premultiplied
  vec4 FinalColor;
#if 1
  FinalColor.a = TexColor.a*lt.a;
  FinalColor.rgb = clamp(TexColor.rgb*FinalColor.a, 0.0, 1.0);
#else
  FinalColor.rgb = TexColor.rgb;
  FinalColor.a = TexColor.a*lt.a;
#endif
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}