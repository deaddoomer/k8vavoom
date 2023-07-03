configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

#add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/strfhelp.o
#  COMMAND ${WINERUN} $<TARGET_FILE:acc> ARGS ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/editing/acs/strfhelp.acs strfhelp.o
#  DEPENDS ${CMAKE_SOURCE_DIR}/utils/editing/acs/*.acs acc
#  )

file(GLOB strife_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/k8vavoom/linedefs/*.rc
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.lmp
  #${CMAKE_CURRENT_BINARY_DIR}/strfhelp.o
  ${CMAKE_SOURCE_DIR}/progs/strife/cgame/*.vc
  ${CMAKE_SOURCE_DIR}/progs/strife/game/*.vc
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    palette.pcx
    basepak.ls.in
    bigfont.pcx
    ${strife_pak_SRC}
  )
add_custom_target(strife_basepak ALL
  DEPENDS
    #${CMAKE_CURRENT_BINARY_DIR}/strfhelp.o
    ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  DESTINATION ${DATADIR}/basev/strife)
