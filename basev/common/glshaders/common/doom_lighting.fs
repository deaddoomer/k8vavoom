// this code is based on GZDoom lighting shader
uniform float LightGlobVis;
uniform int LightMode; // 0: Vavoom; 1: Dark; 2: DarkBanded


//===========================================================================
//
//  Doom-like lighting equation
//
//===========================================================================
float DoomLightingEquation (float llev) {
  // L is the integer llev level used in the game
  float L = llev*255.0;

  #ifdef VAVOOM_REVERSE_Z
    float z = 1.0/gl_FragCoord.w;
  #else
    float z = gl_FragCoord.z/gl_FragCoord.w;
  #endif

  // more-or-less Doom light level equation
  float vis = min(LightGlobVis/z, 24.0/32.0);
  float shade = 2.0-(L+12.0)/128.0;
  float lightscale = shade-vis;

  if (LightMode >= 2) lightscale = (-floor(-lightscale*31.0)-0.5)/31.0; // banded

  lightscale = clamp(lightscale, 0.0, 31.0/32.0);
  // `lightscale` is the normalized colormap index (0 bright .. 1 dark)

  return 1.0-lightscale;
}


#ifdef HAS_GLOW_VARS
vec4 calcGlowLLev (vec4 lt) {
  float llev = lt.a;
  if (LightMode > 0) llev = DoomLightingEquation(llev);
  lt.rgb *= llev;
  lt = calcGlow(lt.rgb, llev); // `lt.a` is always `1.0`
  //lt.a = 1.0;
  return lt;
}
#else
vec4 calcLightLLev (vec4 lt) {
  float llev = lt.a;
  if (LightMode > 0) llev = DoomLightingEquation(llev);
  lt.rgb *= llev;
  lt.a = 1.0;
  return lt;
}
#endif
