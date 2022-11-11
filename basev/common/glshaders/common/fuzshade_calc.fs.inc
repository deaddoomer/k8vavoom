// most code taken from GZDoom; thanks alot!
// based on GZDoom shaders
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
