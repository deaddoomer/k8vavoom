#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform vec4 Light;
uniform vec3 StencilColor;
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
  FinalColor.a = TexColor.a; //*lt.a;
  //FinalColor.rgb *= lt.rgb;
  FinalColor.rgb = StencilColor.rgb;
  $include "common/fog_calc.fs"

  out_FragValue0 = FinalColor;
}
