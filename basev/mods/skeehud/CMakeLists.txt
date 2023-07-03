configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB skeehud_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/progs/*
  ${CMAKE_CURRENT_SOURCE_DIR}/progs/skulldashhud/*
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    ${skeehud_pak_SRC}
  )
add_custom_target(skeehud_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  DESTINATION ${DATADIR}/basev/mods/skeehud)
