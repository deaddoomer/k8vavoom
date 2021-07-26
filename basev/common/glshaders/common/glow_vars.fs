#define HAS_GLOW_VARS
// if a is 0, there is no glow
uniform vec4 GlowColorFloor;
uniform vec4 GlowColorCeiling;

uniform float FloorGlowHeight;
uniform float CeilingGlowHeight;

varying float floorHeight;
varying float ceilingHeight;


vec4 calcGlow (vec3 light, float llev) {
  // glow `.a` is either `0.0` or `1.0` (fullbright)
  float gfa = mix(llev, 1.0, GlowColorFloor.a);
  float gca = mix(llev, 1.0, GlowColorCeiling.a);

  float fh = ((FloorGlowHeight-clamp(abs(floorHeight), 0.0, FloorGlowHeight))*gfa)/FloorGlowHeight;
  float ch = ((CeilingGlowHeight-clamp(abs(ceilingHeight), 0.0, CeilingGlowHeight))*gca)/CeilingGlowHeight;

  /*
  vec4 lt;
  lt.r = clamp(light.r+GlowColorFloor.r*fh+GlowColorCeiling.r*ch, 0.0, 1.0);
  lt.g = clamp(light.g+GlowColorFloor.g*fh+GlowColorCeiling.g*ch, 0.0, 1.0);
  lt.b = clamp(light.b+GlowColorFloor.b*fh+GlowColorCeiling.b*ch, 0.0, 1.0);
  lt.a = 1.0;
  */

  vec3 glowF = GlowColorFloor.rgb*fh;
  vec3 glowC = GlowColorCeiling.rgb*ch;

  vec4 lt = clamp(vec4(light.rgb+glowF+glowC, 1.0), 0.0, 1.0);
  /*
  vec4 lt = clamp(
    vec4(light.r+GlowColorFloor.r*fh+GlowColorCeiling.r*ch,
         light.g+GlowColorFloor.g*fh+GlowColorCeiling.g*ch,
         light.b+GlowColorFloor.b*fh+GlowColorCeiling.b*ch,
         1.0), 0.0, 1.0);
  */
  //lt.rgb = vec3(0.0, 0.0, 1.0);
  return lt;
}
