#version 120
$include "common/common.inc"

$include "common/fog_vars.fs"

uniform sampler2D Texture;
uniform sampler2D Texture2;
uniform float Brightness;

varying vec2 TextureCoordinate;
varying vec2 Texture2Coordinate;


void main () {
  vec4 FinalColor;

  vec4 Tex2 = texture2D(Texture, Texture2Coordinate);
#ifdef VV_OLD_VAVOOM_CLAMPTRANSP
  float ClampTransp2 = clamp((Tex2.a-0.1)/0.9, 0.0, 1.0);
  ClampTransp2 = ClampTransp2*(ClampTransp2*(3.0-(2.0*ClampTransp2)));
#else
  float ClampTransp2 = Tex2.a;
#endif

  vec4 BrightFactor = vec4(Brightness, Brightness, Brightness, 1.0);

  FinalColor = mix(texture2D(Texture2, TextureCoordinate), Tex2, ClampTransp2)*BrightFactor;
  FinalColor.a = 1.0; // sky cannot be translucent

  $include "common/fog_calc.fs"

  out_FragValue0 = FinalColor;
}
