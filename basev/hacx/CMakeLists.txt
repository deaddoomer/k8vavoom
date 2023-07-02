configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB hacx_pak_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/after_iwad/*.txt)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    vfxdefs.txt
    ${hacx_pak_SRC}
  )
add_custom_target(hacx_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/hacx)
