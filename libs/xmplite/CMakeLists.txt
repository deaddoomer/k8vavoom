add_definitions(-DBUILDING_STATIC)
add_definitions(-DLIBXMP_CORE_PLAYER)
add_definitions(-DLIBXMP_NO_PROWIZARD)
add_definitions(-DLIBXMP_NO_DEPACKERS)
add_definitions(-DXMP_SYM_VISIBILITY)
add_definitions(-DHAVE_ROUND)
add_definitions(-DHAVE_POWF)
if(NOT WIN32)
  add_definitions(-DHAVE_LOCALTIME_R)
endif(NOT WIN32)

# TEMP!
set(XMPLITE_C_FLAGS "-Wno-unused-parameter -Wno-sign-compare")

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.9)
  set(XMPLITE_C_FLAGS "${XMPLITE_C_FLAGS} -Wno-shadow=compatible-local -Wno-shadow=local")
endif()

set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_MINSIZEREL} ${XMPLITE_C_FLAGS}")


include_directories(src src/loaders)
add_library(xmplite STATIC
  xmp.h
  src/virtual.c
  src/format.c
  src/period.c
  src/player.c
  src/read_event.c
  src/misc.c
  src/dataio.c
  src/lfo.c
  src/scan.c
  src/control.c
  src/filter.c
  src/effects.c
  src/mixer.c
  src/mix_all.c
  src/load_helpers.c
  src/load.c
  src/filetype.c
  src/hio.c
  src/smix.c
  src/memio.c
  src/win32.c
  src/loaders/common.c
  src/loaders/itsex.c
  src/loaders/sample.c
  src/loaders/xm_load.c
  src/loaders/mod_load.c
  src/loaders/s3m_load.c
  src/loaders/it_load.c
)
