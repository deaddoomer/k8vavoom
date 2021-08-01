#version 120
$include "common/common.inc"

uniform vec4 Light;
uniform sampler2D Texture;
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < ALPHA_MIN) discard;

  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

#ifdef VV_OLD_VAVOOM_CLAMPTRANSP
  float ClampTransp = clamp(((Light.a*TexColor.a)-0.1)/0.9, 0.0, 1.0);
  ClampTransp = ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp)));
#else
  float ClampTransp = clamp(Light.a*TexColor.a, 0.0, 1.0);
#endif

  vec4 FinalColor;
  FinalColor.rgb = Light.rgb;
  FinalColor.a = clamp(InAlpha*ClampTransp, 0.0, 1.0);

  out_FragValue0 = FinalColor;
}
