#version 120

uniform sampler2D Texture;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform float InAlpha;
uniform int FogType;
uniform bool FogEnabled;
uniform bool AllowTransparency;

varying vec4 Light;
varying vec3 VertToView;
varying vec3 VPos;
varying vec2 TextureCoordinate;

void main () {
  float DistVPos = /*sqrt*/(dot(VPos, VPos));
  if (DistVPos < 0.0) discard;

  float DistToView = /*sqrt*/(dot(VertToView, VertToView));
  if (DistToView < 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  if (TexColour.a < 0.01) discard;

  vec4 FinalColour_1 = TexColour;
  $include "common_fog.fs"

  if (!AllowTransparency) {
    if (InAlpha == 1.0 && FinalColour_1.a < 0.666) discard;
  } else {
    if (FinalColour_1.a < 0.01) discard;
  }

  gl_FragColor = FinalColour_1;
}
