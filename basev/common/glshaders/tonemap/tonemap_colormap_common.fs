$include "common/common.inc"

// faster gamma is much cheaper, but assuming gamma of 2.0 instead of 2.2

//#define USE_NO_GAMMA
#define USE_FASTER_GAMMA

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;


#ifndef USE_NO_GAMMA
# ifndef USE_FASTER_GAMMA
// sRGBungamma
// sRGB "gamma" function (approx 2.2)
float sRGBgamma (float v) {
  if (v <= 0.0031308) v *= 12.92; else v = 1.055*pow(v, 1.0/2.4)-0.055;
  return clamp(v, 0.0, 1.0);
}


// sRGBungamma
// inverse of sRGB "gamma" function. (approx 2.2)
float sRGBungamma (float c) {
  if (c <= 0.04045) return c/12.92;
  return pow((c+0.055)/1.055, 2.4);
}
# endif
#endif


float intensity (vec3 c) {
#ifdef USE_NO_GAMMA
  return clamp(c.r*0.2989+c.g*0.5870+c.b*0.1140, 0.0, 1.0);
#else
# ifdef USE_FASTER_GAMMA
  vec3 lc = c*c;
  float i = 0.212655*lc.r+0.715158*lc.g+0.072187*lc.b;
  return clamp(sqrt(i), 0.0, 1.0);
# else
  // sRGB luminance(Y) values
  return sRGBgamma(0.212655*sRGBungamma(c.r)+0.715158*sRGBungamma(c.g)+0.072187*sRGBungamma(c.b));
# endif
#endif
}


vec3 Tonemap (vec3 color) {
  float i = intensity(color);
  return CALCTMAP;
}


void main () {
  vec3 color = texture2D(ScreenFBO, TextureCoordinate).rgb;
  gl_FragColor = vec4(Tonemap(color), 1.0);
}