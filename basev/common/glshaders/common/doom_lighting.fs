// this code is based on GZDoom lighting shader
uniform float LightGlobVis;
uniform float LightMode; // 0: Vavoom; 1: Dark; 2: DarkBanded


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

  // LightGlobVis = R_GetGlobVis(r_visibility) / 32.0
  // Allow negative visibilities, just for novelty's sake
  //return clamp(vis, -204.7, 204.7); // (205 and larger do not work in 5:4 aspect ratio)
  //CUSTOM_CVAR(Float, r_visibility, 8.0f, CVAR_NOINITCALL)

  //float LightGlobVis = 1280.0/32.0; //8.0/32.0; //8.0;

  // more-or-less Doom light level equation
  float vis = min(LightGlobVis/z, 24.0/32.0);
  float shade = 2.0-(L+12.0)/128.0;
  float lightscale;

  if (LightMode >= 2.0) {
    // banded
    lightscale = float(-floor(-(shade-vis)*31.0)-0.5)/31.0;
  } else {
    // smooth
    lightscale = shade-vis;
  }

  // result is the normalized colormap index (1 bright .. 0 dark)
  return 1.0-clamp(lightscale, 0.0, 31.0/32.0);
}


#ifdef HAS_GLOW_VARS
vec4 calcGlowLLev (vec4 lt) {
  float llev = lt.a;
  lt = calcGlow(lt.rgb); // `lt.a` is always `1.0`
  if (LightMode > 0.0) llev = DoomLightingEquation(llev);
  lt.rgb *= llev;
  //lt.a = 1.0;
  return lt;
}
#else
vec4 calcLightLLev (vec4 lt) {
  float llev = lt.a;
  if (LightMode > 0.0) llev = DoomLightingEquation(llev);
  lt.rgb *= llev;
  lt.a = 1.0;
  return lt;
}
#endif
