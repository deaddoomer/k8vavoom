configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB doom_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/croshai*.*
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/croshai*
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.lmp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.imgz
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/k8vavoom/linedefs/*.rc
  ${CMAKE_SOURCE_DIR}/progs/doom/cgame/*.vc
  ${CMAKE_SOURCE_DIR}/progs/doom/game/*.vc
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    bigfont.pcx
    #animated.lmp
    palette.pcx
    ${doom_pak_SRC}
  )
add_custom_target(doom_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/doom)
