#version 120
$include "common/common.inc"

$include "common/fog_vars.fs"

uniform sampler2D Texture;
uniform float Brightness;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColor;

  vec4 BrightFactor = vec4(Brightness, Brightness, Brightness, 1.0);
  FinalColor = texture2D(Texture, TextureCoordinate)*BrightFactor;
  FinalColor.a = 1.0; // sky cannot be translucent

  $include "common/fog_calc.fs"

  out_FragValue0 = FinalColor;
}
