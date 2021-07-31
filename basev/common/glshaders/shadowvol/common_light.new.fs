  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec3 normV2L = normalize(VertToLight);

  // perform shadowmap check later: this way we can avoid checks when the attenuation is zero

  // "half-lambert" lighting model
#ifdef VV_SMAP_SQUARED_DIST
  float DistToLightReal = sqrt(DistToLight);
#else
  DistToLight = sqrt(DistToLight);
  #define DistToLightReal DistToLight
#endif

  float attenuation = (LightRadius-DistToLightReal-LightMin)*(0.5+0.5*dot(normV2L, Normal));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif
  $include "shadowvol/common_light_att_reject.fs"

  // check shadowmaps
#ifdef VV_SHADOWMAPS
  $include "shadowvol/smap_light_check.fs"
#ifdef SHADOWMAPS_HAVE_SHADOWMUL
  attenuation *= shadowMul;
  //if (attenuation <= 0.0) discard;
#endif
#endif

  float finalA = min(attenuation/255.0, 1.0);

#ifdef VV_SHADOW_CHECK_TEXTURE
  float transp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
/*
  #ifdef VV_LIGHT_MODEL
  if (transp < ALPHA_MIN) discard;
  #endif
*/
  finalA *= transp*transp*(3.0-(2.0*transp));
#endif

#ifdef VV_SHADOWMAPS
  //shadowMul = clamp(shadowMul*1.0, 0.0, 1.0); //debug
  //out_FragValue0 = vec4(shadowMul, 0.0, 0.0, 1.0);
  //out_FragValue0 = vec4(clamp(fromLightToFragment.x, 0.0, 1.0), 0.0, 0.0, 1.0);
  out_FragValue0 = vec4(LightColor, finalA);
#else
  out_FragValue0 = vec4(LightColor, finalA);
#endif
