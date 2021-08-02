#version 120
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform float Specular;

$include "common/overbright.fs"


void main () {
  vec3 color = texture2D(ScreenFBO, TextureCoordinate).rgb;

  //const float spcoeff = clampval(r_overbright_specular.asFloat(), 0.0f, 16.0f);
  normOverbrightV3(color, Specular);

  out_FragValue0 = vec4(color, 1.0f);
}
