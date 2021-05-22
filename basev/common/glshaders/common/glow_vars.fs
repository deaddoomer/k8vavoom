// if a is 0, there is no glow
uniform vec4 GlowColorFloor;
uniform vec4 GlowColorCeiling;

uniform float FloorGlowHeight;
uniform float CeilingGlowHeight;

varying float floorHeight;
varying float ceilingHeight;


vec4 calcGlow (vec4 light) {
  float fh = ((FloorGlowHeight-clamp(abs(floorHeight), 0.0, FloorGlowHeight))*GlowColorFloor.a)/FloorGlowHeight;
  float ch = ((CeilingGlowHeight-clamp(abs(ceilingHeight), 0.0, CeilingGlowHeight))*GlowColorCeiling.a)/CeilingGlowHeight;
  vec4 lt;
  lt.r = clamp(light.r+GlowColorFloor.r*fh+GlowColorCeiling.r*ch, 0.0, 1.0);
  lt.g = clamp(light.g+GlowColorFloor.g*fh+GlowColorCeiling.g*ch, 0.0, 1.0);
  lt.b = clamp(light.b+GlowColorFloor.b*fh+GlowColorCeiling.b*ch, 0.0, 1.0);
  lt.a = light.a;

#if 0
  // this doesn't work right
  float i = clamp(lt.r*0.2989+lt.g*0.5870+lt.b*0.1140, 0.0, 1.0);
  if (i < 0.1) {
#ifdef VAVOOM_REVERSE_Z
    float z = 1.0/gl_FragCoord.w;
#else
    float z = gl_FragCoord.z/gl_FragCoord.w;
#endif
    if (z < 64.0) {
      float n = (1.0-z/64.0)*0.5;
      lt.rgb = clamp(lt.rgb+vec3(n, n, n), vec3(0.0), vec3(1.0));
    }
  }
#endif

  return lt;
}
