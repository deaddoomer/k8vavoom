#version 120
//#extension GL_EXT_gpu_shader4 : enable
$include "common/common.inc"

#define LUT_WIDTH   512
#define LUT_HEIGHT  512

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform sampler2D TexPalLUT;


vec3 Tonemap (vec3 color) {
#if 1
  ivec3 c = ivec3(clamp(color.rgb, vec3(0.0), vec3(1.0))*63.0+0.5);
  int index = int((c.r*64+c.g)*64+c.b);
  float tx = int(mod(index, 512))+0.5;
  tx /= 512.0;
  float ty = int(index/512)+0.5;
  ty /= 512.0;
  return texture2D(TexPalLUT, vec2(tx, ty)).rgb;
  //return texture2D(TexPalLUT, vec2(0.0, 0.0)).rgb;
#else
  float i = clamp(color.r*0.2989+color.g*0.5870+color.b*0.1140, 0.0, 1.0);
  return vec3(i, i, i);
#endif
}


void main () {
  vec3 color = texture2D(ScreenFBO, TextureCoordinate).rgb;
  gl_FragColor = vec4(Tonemap(color), 1.0);
}
