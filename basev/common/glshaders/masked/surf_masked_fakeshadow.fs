#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


void main () {
  // no need to calculate shading here
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  // alpha is in `Light.a`
  TexColor.a *= Light.a;
  if (TexColor.a < AlphaRef) discard;

  vec4 lt = Light;
  //TexColor.rgb *= lt.rgb;

  // black-stencil it
  vec4 FinalColor;
  FinalColor.a = TexColor.a; //clamp(TexColor.a*lt.a, 0.0, 1.0);
  FinalColor.rgb = vec3(0.0, 0.0, 0.0);
  $include "common/fog_calc.fs"

  out_FragValue0 = clamp(FinalColor, 0.0, 1.0);
}
