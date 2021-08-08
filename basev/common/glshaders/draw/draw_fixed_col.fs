#version 120
$include "common/common.inc"

uniform vec4 Color;


void main () {
  // we got a non-premultiplied color, convert it
  vec4 FinalColor = vec4(Color.rgb*Color.a, Color.a);
  out_FragValue0 = FinalColor;
}
