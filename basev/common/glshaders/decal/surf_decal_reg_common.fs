// decal renderer for regular case (normal and lightmapped surfaces)

uniform sampler2D Texture;
uniform float SplatAlpha; // image alpha will be multiplied by this
#ifdef REG_LIGHTMAP
$include "common/texlmap_vars.fs"
#else
$include "common/texture_vars.fs"
#endif
#ifdef REG_LIGHTMAP
uniform sampler2D LightMap;
uniform float Specular;
#endif
uniform float FullBright; // for fullbright; either 0.0 or 1.0
//#ifndef REG_LIGHTMAP
uniform vec4 Light;
//#endif
$include "common/fog_vars.fs"
$include "common/glow_vars.fs"
$include "common/doom_lighting.fs"
$include "common/overbright.fs"


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

#ifdef REG_LIGHTMAP
  // lightmapped
/*
  vec3 lmc = texture2D(LightMap, LightmapCoordinate).rgb;
  lmc.r = max(lmc.r, FullBright);
  lmc.g = max(lmc.g, FullBright);
  lmc.b = max(lmc.b, FullBright);
*/

  vec4 lt = vec4(Light.rgb, mix(Light.a, 1.0, FullBright));
  //lt = calcLightLLev(lt);
  lt = calcGlowLLev(lt);

  vec3 lmap = texture2D(LightMap, LightmapCoordinate).rgb;
  lmap.r = mix(lmap.r, 0.0, FullBright);
  lmap.g = mix(lmap.g, 0.0, FullBright);
  lmap.b = mix(lmap.b, 0.0, FullBright);
  lt.rgb += lmap.rgb;
  normOverbrightV4(lt, Specular);

  FinalColor.rgb *= lt.rgb;
#else
  // normal
  vec4 lt = vec4(Light.rgb, mix(Light.a, 1.0, FullBright));
  //lt = calcLightLLev(lt);
  lt = calcGlowLLev(lt);
  FinalColor.rgb *= lt.rgb;
  //FinalColor.rgb *= max(lt.a, FullBright);
#endif
  FinalColor.rgb = clamp(FinalColor.rgb, 0.0, 1.0);

  $include "common/fog_calc.fs"

  //if (FinalColor.a < ALPHA_MIN) discard;

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;

  out_FragValue0 = FinalColor;
}
