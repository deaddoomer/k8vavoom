#version 120
$include "common/common.inc"

// fragment shader for lightmapped surfaces

uniform sampler2D Texture;
uniform sampler2D LightMap;
#ifdef VV_USE_OVERBRIGHT
uniform sampler2D SpecularMap;
#endif
#ifdef VV_LIGHTMAP_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
//$include "common/texshade.inc" // in "texlmap_vars.fs"

$include "common/fog_vars.fs"
$include "common/texlmap_vars.fs"

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
$include "common/doom_lighting.fs"
uniform float FullBright; // 0.0 or 1.0
uniform vec4 Light;
//#endif


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
#ifdef VV_SIMPLE_MASKED
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
#ifdef VV_LIGHTMAP_BRIGHTMAP
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  /*
  vec4 lt = texture2D(LightMap, LightmapCoordinate);
  lt.a = 1.0;
  //lt.a = mix(1.0, Light.a, FullBright); // required for `calcGlowLLev()`
  //lt.a = Light.a; // required for `calcGlowLLev()`
  lt.r = mix(lt.r, Light.r, FullBright);
  lt.g = mix(lt.g, Light.g, FullBright);
  lt.b = mix(lt.b, Light.b, FullBright);
  //lt = calcGlow(lt);
  lt = calcGlowLLev(lt);
  */
  vec4 amblight = vec4(Light.rgb, mix(Light.a, 1.0, FullBright));
  amblight = calcGlowLLev(amblight);
  vec4 lt = texture2D(LightMap, LightmapCoordinate);
  lt.a = 1.0;
  //lt.a = mix(1.0, Light.a, FullBright); // required for `calcGlowLLev()`
  //lt.a = Light.a; // required for `calcGlowLLev()`
  lt.r = mix(lt.r, 0.0, FullBright);
  lt.g = mix(lt.g, 0.0, FullBright);
  lt.b = mix(lt.b, 0.0, FullBright);
  //lt = calcGlow(lt);
  lt.rgb += amblight.rgb;

#ifdef VV_LIGHTMAP_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  //TexColor *= lt;
  TexColor.rgb *= lt.rgb;
#ifdef VV_USE_OVERBRIGHT
  TexColor.rgb += texture2D(SpecularMap, LightmapCoordinate).rgb;
#endif

  // convert to premultiplied
  vec4 FinalColor;
  FinalColor.a = lt.a;//TexColor.a*lt.a; //k8: non-additive and non-translucent should not end here anyway
  FinalColor.rgb = clamp(TexColor.rgb*FinalColor.a, 0.0, 1.0);
  //vec4 FinalColor = TexColor;
  $include "common/fog_calc.fs"

  //TexColor = clamp(TexColor, 0.0, 1.0);
  //vec4 FinalColor = TexColor;
  //$include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}
