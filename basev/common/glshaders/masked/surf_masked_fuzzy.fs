#version 120
$include "common/common.inc"
// most code taken from GZDoom; thanks alot!

uniform sampler2D Texture;
//uniform vec4 Light;
uniform float AlphaRef;
uniform float Time;
uniform int Mode;
uniform int ViewHeight;


$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;

#define PI  3.14159265358979323846


#define FUZZTABLE           50
#define FUZZ_RANDOM_X_SIZE  100
#define FRACBITS            16
#define FRACBITSMUL         65536
#define fixed_t int

int imod (int ia, int ib) {
  float a = float(ia);
  float b = float(ib);
  float m = a-floor((a+0.5)/b)*b;
  return int(floor(m+0.5));
}


int fuzz_random_x_offset[FUZZ_RANDOM_X_SIZE] = int[]
(
  75, 76, 21, 91, 56, 33, 62, 99, 61, 79,
  95, 54, 41, 18, 69, 43, 49, 59, 10, 84,
  94, 17, 57, 46,  9, 39, 55, 34,100, 81,
  73, 88, 92,  3, 63, 36,  7, 28, 13, 80,
  16, 96, 78, 29, 71, 58, 89, 24,  1, 35,
  52, 82,  4, 14, 22, 53, 38, 66, 12, 72,
  90, 44, 77, 83,  6, 27, 48, 30, 42, 32,
  65, 15, 97, 20, 67, 74, 98, 85, 60, 68,
  19, 26,  8, 87, 86, 64, 11, 37, 31, 47,
  25,  5, 50, 51, 23,  2, 93, 70, 40, 45
);

int fuzzoffset[FUZZTABLE] = int[]
(
   6, 11,  6, 11,  6,  6, 11,  6,  6, 11,
   6,  6,  6, 11,  6,  6,  6, 11, 15, 18,
  21,  6, 11, 15,  6,  6,  6,  6, 11, 6,
  11,  6,  6, 11, 15,  6,  6, 11, 15, 18,
  21,  6,  6,  6,  6, 11,  6,  6, 11, 6
);


// based on GZDoom shaders
void main () {
  // no need to calculate shading here
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < AlphaRef) discard;

  //vec4 lt = Light;
  //TexColor.rgb *= lt.rgb;

  // black-stencil it
  /*
  vec4 FinalColor;
  FinalColor.a = clamp(TexColor.a*lt.a, 0.0, 1.0);
  FinalColor.rgb = vec3(0.0, 0.0, 0.0);
  */
  vec4 FinalColor;
  float resFuzzAlpha;

  if (Mode <= 0) {
    // ideally fuzzpos would be an uniform and differ from each sprite so that overlapping demons won't get the same shade for the same pixel
    int next_random = int(abs(mod(Time*16.0, float(FUZZ_RANDOM_X_SIZE))));
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);

    #if 0
    // new glsl
    int fuzzpos = (/*fuzzpos+*/fuzz_random_x_offset[next_random]*FUZZTABLE/100)%FUZZTABLE;
    fixed_t fuzzscale = (200<<FRACBITS)/ViewHeight;
    int scaled_x = (x*fuzzscale)>>FRACBITS;
    int fuzz_x = fuzz_random_x_offset[scaled_x%FUZZ_RANDOM_X_SIZE]+fuzzpos;
    fixed_t fuzzcount = FUZZTABLE<<FRACBITS;
    fixed_t fuzz = ((fuzz_x<<FRACBITS)+y*fuzzscale)%fuzzcount;
    resFuzzAlpha = float(fuzzoffset[fuzz>>FRACBITS])/32.0;
    #else
    int fuzzpos = imod(/*fuzzpos+*/fuzz_random_x_offset[next_random]*FUZZTABLE/100, FUZZTABLE);
    fixed_t fuzzscale = (200*FRACBITSMUL)/ViewHeight;
    int scaled_x = (x*fuzzscale)/FRACBITSMUL;
    int fuzz_x = fuzz_random_x_offset[imod(scaled_x, FUZZ_RANDOM_X_SIZE)]+fuzzpos;
    fixed_t fuzzcount = FUZZTABLE*FRACBITSMUL;
    fixed_t fuzz = imod((fuzz_x*FRACBITSMUL)+y*fuzzscale, fuzzcount);
    resFuzzAlpha = float(fuzzoffset[fuzz/FRACBITSMUL])/32.0;
    #endif
  } else {
    float xtime = Time/2.0;
    float texX, texY;
    if (Mode == 1 || Mode > 3) {
      /* smooth */
      texX = TextureCoordinate.x/3.0+0.66;
      texY = 0.34-TextureCoordinate.y/3.0;
    } else if (Mode == 2) {
      /* smoothnoise */
      texX = sin(mod(TextureCoordinate.x*100.0+xtime*5.0, 3.489))+TextureCoordinate.x/4.0;
      texY = cos(mod(TextureCoordinate.y*100.0+xtime*5.0, 3.489))+TextureCoordinate.y/4.0;
    } else if (Mode == 3) {
      /* jagged */
      vec2 texSplat = vec2(
        TextureCoordinate.x+mod(sin(PI*2.0*(TextureCoordinate.y+xtime*2.0)),0.1)*0.1,
        TextureCoordinate.y+mod(cos(PI*2.0*(TextureCoordinate.x+xtime*2.0)),0.1)*0.1);
      texX = sin(TextureCoordinate.x*100.0+xtime*5.0);
      texY = cos(TextureCoordinate.x*100.0+xtime*5.0);
    }
    float vX = (texX/texY)*21.0;
    float vY = (texY/texX)*13.0;
    resFuzzAlpha = mod(xtime*2.0+(vX+vY), 0.5)*1.6;
  }

  FinalColor = vec4(0.0, 0.0, 0.0, TexColor.a*resFuzzAlpha);
  $include "common/fog_calc.fs"

  out_FragValue0 = clamp(FinalColor, 0.0, 1.0);
}
