add_subdirectory(core)
add_subdirectory(vavoomc)

#if(NOT WITH_SYS_LZMA)
#  add_subdirectory(liblzma)
#endif(NOT WITH_SYS_LZMA)

if(NOT WITH_SYS_LIBJPG)
  add_subdirectory(jpeg)
endif(NOT WITH_SYS_LIBJPG)

#if(NOT WITH_SYS_ZLIB)
#  add_subdirectory(zlib)
#endif(NOT WITH_SYS_ZLIB)

if(ENABLE_VCCRUN)
  add_subdirectory(imago)
endif(ENABLE_VCCRUN)

if(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY)
  if(WITH_GLAD AND NOT GLAD_LIBRARY)
    message(STATUS "Local GLAD enabled")
    add_subdirectory(glad)
  endif(WITH_GLAD AND NOT GLAD_LIBRARY)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY)

if(ENABLE_CLIENT AND NOT ENABLE_VCCRUN_ONLY)
  add_subdirectory(timidity)
endif(ENABLE_CLIENT AND NOT ENABLE_VCCRUN_ONLY)

if(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)
  add_subdirectory(xmplite)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)

if(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)
  if(WITH_FLUID AND NOT WITH_SYS_FLUID)
    add_subdirectory(fluidsynth_lite)
  endif(WITH_FLUID AND NOT WITH_SYS_FLUID)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)

if(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)
  if(OPUS_USE_INTERNAL)
    add_subdirectory(opus)
  endif(OPUS_USE_INTERNAL)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)
