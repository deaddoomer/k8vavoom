#pragma optionNV(inline all)
#pragma optionNV(unroll all)

#define ALPHA_MIN     (0.012)
// shaders can get "semi-translucent" textures; reject all translucent pixels
#define ALPHA_MASKED  (1.0)
