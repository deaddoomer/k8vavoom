#version 130
$include "common/common.inc"

varying vec2 TextureCoordinate;
uniform sampler2D ScreenFBO;
uniform float SharpenIntensity;


// Adapted from https://github.com/DadSchoorse/vkBasalt/blob/b929505ba71dea21d6c32a5a59f2d241592b30c4/src/shader/cas.frag.glsl
// (MIT license).
vec3 do_cas (vec3 color, vec2 uv_interp, float sharpen_intensity) {
  // Fetch a 3x3 neighborhood around the pixel 'e',
  //  a b c
  //  d(e)f
  //  g h i
  vec3 a = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(-1, -1)).rgb;
  vec3 b = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(0, -1)).rgb;
  vec3 c = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(1, -1)).rgb;
  vec3 d = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(-1, 0)).rgb;
  vec3 e = color.rgb;
  vec3 f = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(1, 0)).rgb;
  vec3 g = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(-1, 1)).rgb;
  vec3 h = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(0, 1)).rgb;
  vec3 i = textureLodOffset(ScreenFBO, uv_interp, 0.0, ivec2(1, 1)).rgb;

  // Soft min and max.
  //  a b c             b
  //  d e f * 0.5  +  d e f * 0.5
  //  g h i             h
  // These are 2.0x bigger (factored out the extra multiply).
  vec3 min_rgb = min(min(min(d, e), min(f, b)), h);
  vec3 min_rgb2 = min(min(min(min_rgb, a), min(g, c)), i);
  min_rgb += min_rgb2;

  vec3 max_rgb = max(max(max(d, e), max(f, b)), h);
  vec3 max_rgb2 = max(max(max(max_rgb, a), max(g, c)), i);
  max_rgb += max_rgb2;

  // Smooth minimum distance to signal limit divided by smooth max.
  vec3 rcp_max_rgb = vec3(1.0) / max_rgb;
  vec3 amp_rgb = clamp((min(min_rgb, 2.0 - max_rgb) * rcp_max_rgb), 0.0, 1.0);

  // Shaping amount of sharpening.
  amp_rgb = inversesqrt(amp_rgb);
  float peak = 8.0 - 3.0 * sharpen_intensity;
  vec3 w_rgb = -vec3(1) / (amp_rgb * peak);
  vec3 rcp_weight_rgb = vec3(1.0) / (1.0 + 4.0 * w_rgb);

  //                          0 w 0
  //  Filter shape:           w 1 w
  //                          0 w 0
  vec3 window = b + d + f + h;

  return max(vec3(0.0), (window * w_rgb + e) * rcp_weight_rgb);
}


void main () {
  //vec3 color = texture2D(ScreenFBO, fragCoord).rgb;
  //vec3 color = textureLod(ScreenFBO, vec3(TextureCoordinate, /*ViewIndex*/0.0f), 0.0f).rgb;
  vec3 color = textureLod(ScreenFBO, TextureCoordinate, 0.0f).rgb;

  color = do_cas(color, TextureCoordinate, SharpenIntensity);

  out_FragValue0 = vec4(color, 1.0f);
}
