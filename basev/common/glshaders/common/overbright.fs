// calc additive light (overbright)
float normOverbright (float v, float spcoeff) {
  if (v > 1.0) v = 1.0+min((v-1.0), 1.0)*(spcoeff*4.0);
  return v;
}

void normOverbrightV3 (inout vec3 color, float spcoeff) {
  color.r = normOverbright(color.r, spcoeff);
  color.g = normOverbright(color.g, spcoeff);
  color.b = normOverbright(color.b, spcoeff);
}

void normOverbrightV4 (inout vec4 color, float spcoeff) {
  color.r = normOverbright(color.r, spcoeff);
  color.g = normOverbright(color.g, spcoeff);
  color.b = normOverbright(color.b, spcoeff);
}
