#version 110

uniform sampler2D	Texture;
uniform int			FogType;
uniform vec4		FogColour;
uniform float		FogDensity;
uniform float		FogStart;
uniform float		FogEnd;

varying vec2		TextureCoordinate;
varying vec3		VertToView;

uniform float		InAlpha;

void main()
{
	float DistToView = length(VertToView);

	if (DistToView <= 0.0)
	{
		discard;
	}

	vec4 FinalColour = texture2D(Texture, TextureCoordinate);
	if (FinalColour.a <= 0.1)
	{
		discard;
	}

	float z = gl_FragCoord.z / gl_FragCoord.w;
	const float LOG2 = 1.442695;
	float FogFactor;
	if (FogType == 3)
	{
		FogFactor = exp2(-FogDensity * FogDensity * z * z * LOG2);
	}
	else if (FogType == 2)
	{
		FogFactor = exp2(-FogDensity * z * LOG2);
	}
	else
	{
		FogFactor = ((FogEnd - z) / (FogEnd - FogStart));
	}
	FogFactor = clamp(1.0 - FogFactor, 0.0, 1.0) * InAlpha;
	gl_FragColor = vec4(FogColour.rgb, smoothstep(0.1, 1.0, FogFactor * FinalColour.a));
}
