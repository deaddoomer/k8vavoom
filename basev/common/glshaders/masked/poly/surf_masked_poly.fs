#version 120
$include "common/common.inc"

// used to render translucent walls

uniform sampler2D Texture;
$include "common/texshade.inc"
#ifdef VV_MASKED_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
uniform vec4 Light; // .a is light level
uniform float Alpha;
uniform float AlphaRef;

$include "common/fog_vars.fs"

//varying vec2 TextureCoordinate;
$include "common/texture_vars.fs"

#ifdef VV_MASKED_GLOW
$include "common/glow_vars.fs"
#endif
$include "common/doom_lighting.fs"


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  // AlphaRef == -1: special mode: only alpha pixels
  if (AlphaRef < 0.0) {
    if (TexColor.a < 0.01 || TexColor.a >= 1.0) discard;
  } else {
    if (TexColor.a < AlphaRef) discard;
  }
  //TexColor *= Light;

#ifdef VV_MASKED_GLOW
  vec4 lt = calcGlowLLev(Light);
#else
  //vec4 lt = Light;
  vec4 lt = calcLightLLev(Light);
#endif
#ifdef VV_MASKED_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  //TexColor *= lt;
  TexColor.rgb *= lt.rgb;

  // convert to premultiplied
  vec4 FinalColor;
#if 1
  FinalColor.a = TexColor.a* /*lt.a*/Alpha;
  if (TexColor.a < 0.01) discard;
  FinalColor.rgb = clamp(TexColor.rgb*FinalColor.a, 0.0, 1.0);
#else
  FinalColor.rgb = TexColor.rgb;
  FinalColor.a = TexColor.a* /*lt.a*/Alpha;
#endif
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}
