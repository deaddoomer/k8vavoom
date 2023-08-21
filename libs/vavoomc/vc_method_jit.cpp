//**************************************************************************
//
// JIT compiler
//
//**************************************************************************
#if 1 && defined(USE_LIBJIT)
# undef USE_LIBJIT
#endif

#ifdef USE_LIBJIT
# include "vc_method_jit_real.cpp"
#else
void VMethod::CompileToNativeCode () {}
#endif
