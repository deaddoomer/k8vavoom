#version 120
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform float InputTime;
//uniform vec2 TextureDimensions;

// based on the code by Jason Allen Doucette ( http://xona.com/jason/ )
// Quake-style Underwater Distortion
// modified by Ketmar Dark

#define speed  (1.6)

// the amount of shearing (shifting of a single column or row)
// 1.0 = entire screen height offset (to both sides, meaning it's 2.0 in total)
#define xDistMag  (0.008-0.002)
#define yDistMag  (0.006-0.002)

// cycle multiplier for a given screen height
// 2*PI = you see a complete sine wave from top..bottom
#define xSineCycles  (6.28)
#define ySineCycles  (6.28)


void main () {
  vec2 fragCoord = TextureCoordinate.xy;

  float time = InputTime*speed;
  float xAngle = time+fragCoord.y*ySineCycles;
  float yAngle = time+fragCoord.x*xSineCycles;

  vec2 distortOffset =
        vec2(sin(xAngle), sin(yAngle))* // amount of shearing
        vec2(xDistMag, yDistMag); // magnitude adjustment

  // shear the coordinates
  fragCoord += distortOffset;

  //fragCoord.x = fract(fragCoord.x+1024.0);
  //fragCoord.y = fract(fragCoord.y+1024.0);

  gl_FragColor = texture2D(ScreenFBO, fragCoord);
}
