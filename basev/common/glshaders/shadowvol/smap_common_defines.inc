#ifdef VV_SMAP_BLUR8_FAST
# define VV_SMAP_BLUR8
# define VV_SMAP_BLUR_FAST8
#endif

#ifdef VV_SMAP_BLUR16_FAST
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST8
#endif

#ifdef VV_SMAP_BLUR16_FASTER
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST16
#endif

#ifdef VV_SMAP_BLUR16_FASTEST
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST8
# define VV_SMAP_BLUR_FAST16
#endif


// for each next blur, previous one must be defined
//#define VV_SMAP_BLUR4
//#define VV_SMAP_BLUR8
//#define VV_SMAP_BLUR16

// you can define VV_SMAP_BLUR_FAST16 without VV_SMAP_BLUR_FAST8 here
//#define VV_SMAP_BLUR_FAST8
//#define VV_SMAP_BLUR_FAST16

// ensure proper defines
#ifdef VV_SMAP_BLUR16
# ifndef VV_SMAP_BLUR8
#  define VV_SMAP_BLUR8
# endif
#endif

#ifdef VV_SMAP_BLUR8
# ifndef VV_SMAP_BLUR4
#  define VV_SMAP_BLUR4
# endif
#endif

#ifdef VV_SMAP_BLUR16
# define DCOUNT 17.0
#endif

#ifndef DCOUNT
# ifdef VV_SMAP_BLUR8
#  define DCOUNT 9.0
# endif
#endif

#ifndef DCOUNT
# ifdef VV_SMAP_BLUR4
#  define DCOUNT 5.0
# endif
#endif

#ifndef DCOUNT
# define DCOUNT 5.0
#endif

#ifndef VV_DYNAMIC_DCOUNT
# ifdef VV_SMAP_BLUR_FAST8
#  define VV_DYNAMIC_DCOUNT
# endif
#endif

#ifndef VV_DYNAMIC_DCOUNT
# ifdef VV_SMAP_BLUR_FAST16
#  define VV_DYNAMIC_DCOUNT
# endif
#endif

#ifdef VV_DYNAMIC_DCOUNT
# ifndef VV_SMAP_BLUR8
#  undef VV_DYNAMIC_DCOUNT
# endif
#endif

#define VV_BLUR4_MUL(n_)  ((n_)*1.4)
#define VV_BLUR8_MUL(n_)  ((n_)*1.4)
#define VV_BLUR16_MUL(n_)  ((n_)*1.8)