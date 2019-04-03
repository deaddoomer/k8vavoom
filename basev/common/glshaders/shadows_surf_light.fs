#version 120
$include "common/common.inc"

uniform vec3 LightColour;
uniform float LightRadius;
uniform float LightMin;
uniform sampler2D Texture;
#ifdef VV_SPOTLIGHT
uniform vec3 ConeDirection;
uniform float ConeAngle; // degrees
#endif

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

$include "common/texture_vars.fs"


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.1) discard; //FIXME
  if (TexColour.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this

  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  DistToLight = sqrt(DistToLight);

#ifdef VV_SPOTLIGHT
  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
  //float attenuation = 1.0/(1.0+(LightRadius-LightMin)*DistToLight);
  //attenuation *= 255.0;
  //attenuation = 1.0 / (1.0 + light.attenuation * pow(distanceToLight, 2));

  // cone restrictions (affects attenuation)
  vec3 surfaceToLight = normalize(VertToLight);
  float distanceToLight = length(VertToLight);

  //vec3 ConeDirection = vec3(1.0, 0.0, 0.0);
  //float ConeAngle = 50.0;

  //float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, normalize(ConeDirection))));
  float lightToSurfaceAngle = degrees(acos(dot(-surfaceToLight, ConeDirection)));
  if (lightToSurfaceAngle > ConeAngle) attenuation = 0.0;
#else
  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
#endif

  if (attenuation <= 0.0) discard;

  float ClampAdd = min(attenuation/255.0, 1.0);
  attenuation = ClampAdd;

  float Transp = clamp((TexColour.a-0.1)/0.9, 0.0, 1.0);

  vec4 FinalColour_1;
#ifdef VV_DEBUG_LIGHT
  //FinalColour_1 = vec4(1.0, 0.5, 0.5, 1.0);
  FinalColour_1.rgb = LightColour;
  FinalColour_1.a = 1.0;
#else
  FinalColour_1.rgb = LightColour;
  FinalColour_1.a = ClampAdd*(Transp*(Transp*(3.0-(2.0*Transp))));
  //if (FinalColour_1.a < 0.01) discard;
#endif

  gl_FragColor = FinalColour_1;
}
