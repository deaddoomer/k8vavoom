#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/fog_vars.fs"
uniform float InAlpha;
uniform bool AllowTransparency;
#ifdef VV_STENCIL
uniform vec3 StencilColor;
#endif
varying vec4 Light;
varying vec2 TextureCoordinate;

#ifdef VV_FUZZY
$include "common/fuzshade_vars.fs.inc"
#endif


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

#ifdef VV_FUZZY
  $include "common/fuzshade_calc.fs.inc"
  vec4 FinalColor = clamp(vec4(0.0, 0.0, 0.0, TexColor.a*resFuzzAlpha), 0.0, 1.0);
#else
  TexColor *= Light;
  vec4 FinalColor = TexColor;

  // do fog before premultiply, otherwise it is wrong
  //$include "common/fog_calc.fs"

  // convert to premultiplied
  FinalColor.a *= InAlpha;
  if (FinalColor.a < ALPHA_MIN) discard;
#ifdef VV_STENCIL
  FinalColor.rgb = StencilColor.rgb;
#else
  FinalColor.rgb = clamp(FinalColor.rgb*FinalColor.a, 0.0, 1.0);
#endif
#endif

  $include "common/fog_calc.fs"

  out_FragValue0 = FinalColor;
}
