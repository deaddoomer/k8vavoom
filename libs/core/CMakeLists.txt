if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DVAVOOM_DEBUG_BUILD=1)
endif()

if(WITH_MIMALLOC)
  set(CORELIB_MIMALLOC_SOURCES
    mimalloc/mimalloc.h
    mimalloc/mimalloc-atomic.h
    mimalloc/mimalloc-internal.h
    mimalloc/mimalloc-types.h
    mimalloc/mimalloc.h
    mimalloc/static.c
  )
endif()

add_library(core STATIC
  propp.h
  args.cpp
  args.h
  array.h
  binheap.h
  common.h
  common.cpp
  core.h
  colorutil.h
  colorutil.cpp
  endianness.h
  endianness.cpp
  exception.h
  exception.cpp
  log.h
  log.cpp
  map_impl.h
  map.h
  name.h
  names.h
  name.cpp
  ryu.h
  ryu_d2s.c
  ryu_f2s.c
  str.h
  vstrci.h
  str.cpp
  stream/stream.h
  stream/stream.cpp
  stream/streamsbit.h
  stream/streamsbit.cpp
  stream/streamsex.h
  stream/streamsex.cpp
  stream/streamszlib.h
  stream/streamszlib.cpp
  stream/streamvwad.h
  stream/streamvwad.cpp
  stream/ntvalue.cpp
  textparser.h
  textparser.cpp
  mathutil.h
  vecmat/vector_avec.h
  vecmat/vector_3d.h
  vecmat/vector_3d.cpp
  vecmat/vertex2d.h
  vecmat/plane_3d.h
  vecmat/plane_3d.cpp
  vecmat/frustum.h
  vecmat/frustum.cpp
  #vecmat/vector.h
  #vecmat/vector.cpp
  vecmat/matrix4.h
  vecmat/matrixrot.h
  vecmat/matrixvp.h
  vecmat/matrix4.cpp
  vsqlite3/sqlite3.h
  vsqlite3/sqlite3.c
  vsqlite3/vsqlite3_opts.h
  xml.h
  xml.cpp
  zone.h
  zone.cpp
  minipng.h
  minipng.cpp
  syslow.h
  syslow.cpp
  prngs.cpp
  timsort-impl.h
  timsort.h
  timsort.c
  smsort.c
  smsort.h
  strtod_plan9.h
  strtod_plan9.c
  micropather.h
  micropather.cpp
  cvarsys.h
  cvarsys.cpp

  rawtty.cpp


  libvwad/vwadvfs.c
  libvwad/vwadvfs.h
  libvwad/vwadwrite.c
  libvwad/vwadwrite.h


  hash/hashfunc.h
  hash/lookup2.cpp
  hash/lookup3.cpp
  hash/halfsiphash.cpp
  hash/murmur3.cpp
  hash/crc.h
  hash/crc.cpp
  hash/SpookyV2.h
  hash/SpookyV2.c
  hash/xxhash.h
  hash/xxhash.cpp
  hash/rg32.h
  hash/rg32.cpp
  hash/md5.h
  hash/md5.cpp

  crypto/prng_randombytes.h
  crypto/prng_randombytes.c
  crypto/monocypher-ama.h
  crypto/monocypher-ama.c
  crypto/ripemd160.h
  crypto/ripemd160.c

  # mimalloc
  ${CORELIB_MIMALLOC_SOURCES}

  # lzma decoder
  lzmadec/LzmaDec.h
  lzmadec/LzmaDec.c

  # miniz
  miniz/miniz.h
  miniz/miniz.c

  # fsys (aka VFS)
  fsys/fsys.h
  fsys/fsys.cpp
  fsys/fsys_local.h
  fsys/fsys_pakbase.cpp
  fsys/fsys_vfs.cpp
  # formats
  fsys/formats/fsys_dirpak.cpp
  fsys/formats/fsys_wad.cpp
  fsys/formats/fsys_zip.cpp
  fsys/formats/fsys_qkpak.cpp
  fsys/formats/fsys_vwad.cpp
  #fsys/formats/fsys_dfwad.cpp
  # included directly in "fsys.cpp", otherwise "smart" linker will throw it away
  #fsys/fsys_register.cpp
)
