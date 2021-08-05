#version 120
$include "common/common.inc"

// vertical crossfade

uniform sampler2D Texture;
uniform float WipeTime; // in seconds
uniform float WipeDuration; // in seconds
uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


#define STP_HEIGHT  (256.0)


void main () {
  vec4 FinalColor;
  vec4 TexColor;

  TexColor = texture2D(Texture, TextureCoordinate);
  FinalColor.rgb = TexColor.rgb;

  // [0..1]
  float frc = clamp(WipeTime, 0.0, WipeDuration)/WipeDuration;

  float y0 = (ScreenSize.y+STP_HEIGHT)*frc-STP_HEIGHT;
  float y1 = y0+STP_HEIGHT-1.0;

  if (gl_FragCoord.y < y0) {
    FinalColor.a = 1.0;
  } else if (gl_FragCoord.y > y1) {
    FinalColor.a = 0.0;
  } else {
    frc = (gl_FragCoord.y-y0)/STP_HEIGHT;
    FinalColor.a = mix(0.0, 1.0, 1.0-frc);
  }

  FinalColor.a = 1.0-FinalColor.a;

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;

  out_FragValue0 = FinalColor;
}
