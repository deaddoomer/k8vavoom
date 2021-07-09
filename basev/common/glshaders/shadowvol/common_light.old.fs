  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec3 normV2L = normalize(VertToLight);

#ifndef VV_SMAP_SQUARED_DIST
  DistToLight = sqrt(DistToLight);
#endif

#ifdef VV_SHADOWMAPS
  $include "shadowvol/smap_light_check.fs"
#endif

  // "half-lambert" lighting model
#ifdef VV_SMAP_SQUARED_DIST
  DistToLight = sqrt(DistToLight);
#endif
  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+0.5*dot(normV2L, Normal));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

#ifdef VV_SHADOWMAPS
#ifdef SHADOWMAPS_HAVE_SHADOWMUL
  attenuation *= shadowMul;
#endif
  //attenuation = 255.0*shadowMul; // debug
#endif
  $include "shadowvol/common_light_att_reject.fs"

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
  //gl_FragColor = vec4(shadowMul, 0.0, 0.0, 1.0);
  //gl_FragColor = vec4(clamp(fromLightToFragment.x, 0.0, 1.0), 0.0, 0.0, 1.0);
  gl_FragColor = vec4(LightColor, finalA);
#else
  gl_FragColor = vec4(LightColor, finalA);
#endif
