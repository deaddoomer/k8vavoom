configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB chex_pak_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.txt)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    ${CMAKE_CURRENT_SOURCE_DIR}/language.en
    ${chex_pak_SRC}
  )
add_custom_target(chex_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/chex)
