#version 120
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform vec4 MatR0;
uniform vec4 MatR1;
uniform vec4 MatR2;


void main () {
  vec3 color = texture2D(ScreenFBO, TextureCoordinate).rgb;

  vec3 FragColor;
  FragColor.r = color.r*MatR0.r+color.g*MatR0.g+color.b*MatR0.b+MatR0.a;
  FragColor.g = color.r*MatR1.r+color.g*MatR1.g+color.b*MatR1.b+MatR1.a;
  FragColor.b = color.r*MatR2.r+color.g*MatR2.g+color.b*MatR2.b+MatR2.a;

  out_FragValue0 = vec4(clamp(FragColor, 0.0, 1.0), 1.0);
}
