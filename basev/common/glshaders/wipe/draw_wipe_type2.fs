#version 120
$include "common/common.inc"

// fullscreen crossfade

uniform sampler2D Texture;
uniform float WipeTime; // in seconds
uniform float WipeDuration; // in seconds
//uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColor;
  vec4 TexColor;

  TexColor = texture2D(Texture, TextureCoordinate);
  FinalColor.rgb = TexColor.rgb;

  // [0..1]
  float frc = 1.0-clamp(WipeTime, 0.0, WipeDuration)/WipeDuration;

  FinalColor.a = frc;

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;

  out_FragValue0 = FinalColor;
}
