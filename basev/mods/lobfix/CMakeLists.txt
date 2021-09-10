configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB lobfix_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/progs/*
  ${CMAKE_CURRENT_SOURCE_DIR}/progs/lobfix/*
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    ${lobfix_pak_SRC}
  )
add_custom_target(lobfix_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/mods/lobfix)
