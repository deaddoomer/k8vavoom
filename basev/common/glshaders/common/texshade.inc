#ifdef VV_TEXTURED_ZDOOM_SHADE_EX
// texture shade; red channel is used as intensity.
// this is how it was done in ZDoom, sadly.
// anyway, one should not shade anything that is not monochrome.
// this "kinda works" for red blood decals too.
// `a`: 0 is "normal colors", 1 is "shaded"
uniform vec3 TexShade;
uniform float TexShadeMode;

/* VV_TEXTURED_ZDOOM_SHADE
vec3 TexShade;
vec4 GetStdTexelShaded (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  clr.a = clamp(clr.r*SplatAlpha, 0.0, 1.0); // red channel is intensity
  clr.rgb = TexShade.rgb;
  return clr;
}
*/

vec4 GetStdTexelShaded (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  /*
  clr.g = 0.0;
  clr.b = 0.0;
  clr.a = clr.r*clr.a;
  clr.a *= SplatAlpha;
  return clr;
  */
  float smode = TexShadeMode;
  float nmode = 1.0-smode;
  clr.a = clr.a*clr.r*smode+clr.a*nmode; // red channel is intensity
  clr.rgb = TexShade.rgb*smode+clr.rgb*nmode;
  clr.a *= SplatAlpha;
  return clr;
}

#else
# ifdef VV_TEXTURED_ZDOOM_STENCIL
// texture stenciling code
// this is how it was done in ZDoom
uniform vec3 TexShade;

vec4 GetStdTexelShaded (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  float i = clr.r*0.2989+clr.g*0.5870+clr.b*0.1140;
  clr.rgb = clamp(TexShade.rgb*i, vec3(0.0, 1.0));
  clr.a = clamp(clr.a*SplatAlpha, 0.0, 1.0);
  return clr;
}

# else
#  ifdef VV_TEXTURED_ZDOOM_SHADE_STENCIL
// texture shading and stenciling code, slowest, but universal
// this is how it was done in ZDoom
// a component:
//   0: no color change
//  -1: stencil
//   1: shade
uniform vec4 TexShade;

vec4 GetStdTexelShaded (sampler2D smp, vec2 tcoord) {
  vec4 clr = texture2D(smp, tcoord);
  if (TexShade.a < 0.0) {
    float i = clr.r*0.2989+clr.g*0.5870+clr.b*0.1140;
    clr.rgb = clamp(TexShade.rgb*i, vec3(0.0, 1.0));
    clr.a = clamp(clr.a*SplatAlpha, 0.0, 1.0);
  } else if (TexShade.a > 0.0) {
    clr.a = clamp(clr.r*SplatAlpha, 0.0, 1.0); // red channel is intensity
    clr.rgb = TexShade.rgb;
  }
  return clr;
}

#  else
#   define GetStdTexel  texture2D
#  endif
# endif
#endif
