#version 120
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform int Mode;


void main () {
  vec3 color = texture2D(ScreenFBO, TextureCoordinate).rgb;

  vec3 FragColor = color.rgb;
       if (Mode == 1) FragColor = color.gbr;
  else if (Mode == 2) FragColor = color.grb;
  else if (Mode == 3) FragColor = color.brg;
  else if (Mode == 4) FragColor = color.bgr;
  else if (Mode == 5) FragColor = color.rbg;

  out_FragValue0 = vec4(FragColor, 1.0);
}
