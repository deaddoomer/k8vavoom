#pragma optionNV(inline all)
#pragma optionNV(unroll all)

#define ALPHA_MIN     (0.012)
// shaders can get "semi-translucent" textures; reject all translucent pixels
#define ALPHA_MASKED  (1.0)

#ifdef VV_MRT_RENDERING
# define out_FragValue0  gl_FragData[0]
# define out_FragValue1  gl_FragData[1]
# define out_FragValue2  gl_FragData[2]
# define out_FragValue3  gl_FragData[3]
#else
# define out_FragValue0  gl_FragColor
#endif
