include_directories(../common ${ZLIB_INCLUDE_DIR})

add_executable(vlumpy
  ../common/cmdlib.cpp
  ../common/cmdlib.h
  ../common/ftexdefs.h
  ../common/fwaddefs.h
  ../common/imglib.cpp
  ../common/imglib.h
  ../common/scrlib.cpp
  ../common/scrlib.h
  ../common/wadlib.cpp
  ../common/wadlib.h

  ioapi.cpp
  ioapi.h
  vlumpy.cpp
  zip.cpp
  zip.h
)
target_link_libraries(vlumpy core ${ZLIB_LIBRARIES} ${VAVOOM_SHITDOZE_LIBS})
set_target_properties(vlumpy PROPERTIES OUTPUT_NAME ../bin/k8vlumpy)
#add_dependencies(vlumpy core ${LIBLZMA_LIBRARIES} ${ZLIB_DEPS})
add_dependencies(vlumpy core ${ZLIB_DEPS})

if(CYGWIN OR MINGW)
  set_target_properties(vlumpy PROPERTIES LINK_FLAGS "-Wl,--subsystem,console")
  install(TARGETS vlumpy DESTINATION ${BINDIR}/tools)
else(CYGWIN OR MINGW)
  install(TARGETS vlumpy DESTINATION ${BINDIR})
endif()
