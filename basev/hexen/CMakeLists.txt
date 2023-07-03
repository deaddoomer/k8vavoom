configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB hexen_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_SOURCE_DIR}/progs/hexen/cgame/*.vc
  ${CMAKE_SOURCE_DIR}/progs/hexen/game/*.vc
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    graphics/croshai1.lmp
    graphics/croshai2.lmp
    graphics/croshai3.lmp
    graphics/fonta30.lmp
    graphics/fonta63.lmp
    palette.pcx
    basepak.ls.in
    ${hexen_pak_SRC}
  )
add_custom_target(hexen_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  DESTINATION ${DATADIR}/basev/hexen)
