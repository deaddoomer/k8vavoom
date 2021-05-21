#version 120
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform sampler2D TexPalLUT;


vec3 Tonemap (vec3 color) {
#if 1
  ivec3 c = ivec3(clamp(color.rgb, vec3(0.0), vec3(1.0))*127.0+0.5);
  int index = int((c.r*128+c.g)*128+c.b);
  float tx = int(mod(index, 2048))+0.5;
  tx /= 2048.0;
  float ty = int(index/2048)+0.5;
  ty /= 2048.0;
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
