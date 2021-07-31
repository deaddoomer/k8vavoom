#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform float FullBright;
uniform vec2 ScreenSize;

$include "common/texture_vars.fs"


void main () {
  vec4 FinalColor;
  vec4 TexColor;

  //!if (SplatAlpha <= ALPHA_MIN) discard;

  /*
  TexColor = GetStdTexel(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;

  FinalColor.a = clamp(TexColor.a*SplatAlpha, 0.0, 1.0);
  if (FinalColor.a < ALPHA_MIN) discard;
  FinalColor.rgb = TexColor.rgb;
  */
  FinalColor = GetStdTexelShaded(Texture, TextureCoordinate);
  if (FinalColor.a < ALPHA_MIN) discard;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec3 ambColor = texture2D(AmbLightTexture, tc2).rgb;
  // if `FullBright` is 1, mul by 1.0, otherwise mul by ambient
  ambColor.r = 1.0*FullBright+(1.0-FullBright)*ambColor.r;
  ambColor.g = 1.0*FullBright+(1.0-FullBright)*ambColor.g;
  ambColor.b = 1.0*FullBright+(1.0-FullBright)*ambColor.b;
  FinalColor.rgb = clamp(FinalColor.rgb*ambColor.rgb, 0.0, 1.0);

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;

  //FinalColor = ambColor;
  out_FragValue0 = FinalColor;
}
