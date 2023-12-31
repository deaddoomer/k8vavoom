#version 120
$include "common/common.inc"
//#define VV_DEBUG_NORMALS

uniform sampler2D Texture;
//uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;
//uniform vec2 ScreenSize;
#ifdef VV_STENCIL
uniform vec3 StencilColor;
#endif

varying vec2 TextureCoordinate;


#ifdef VV_DEBUG_NORMALS
varying vec3 Normal;
#endif


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

  if (TexColor.a*InAlpha < ALPHA_MIN) discard;

  vec4 FinalColor;

#ifdef VV_STENCIL
  FinalColor.rgb = StencilColor.rgb;
#else
  FinalColor.rgb = TexColor.rgb;
#endif
  FinalColor.a = TexColor.a;

#ifdef VV_OLD_VAVOOM_CLAMPTRANSP
  float ClampTransp = clamp((FinalColor.a-0.1)/0.9, 0.0, 1.0);
  FinalColor.a *= ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp)));
#endif
  FinalColor.a *= InAlpha;

#ifdef VV_DEBUG_NORMALS
  FinalColor.x = Normal.y;
  FinalColor.y = Normal.x;
  FinalColor.z = Normal.z;
  //if (FinalColor.z < 0) { FinalColor.r = 1.0; FinalColor.z = -FinalColor.z; }
#endif

  out_FragValue0 = clamp(FinalColor, 0.0, 1.0);
}
