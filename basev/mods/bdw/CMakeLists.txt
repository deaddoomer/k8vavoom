configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB bdw_pak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/gl/*.gl
  ${CMAKE_CURRENT_SOURCE_DIR}/models/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/tracer/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_EmptyBrass/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_EmptyClip/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_Rifle/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_Shotgun/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_ShotgunCasing/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/voxels/BDW_UnderwaterEmptyBrass/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sounds/weapons/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/ammo/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/casing/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/decals/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/muzzle/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/water/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/weapons/arifle/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/weapons/shotgun/*
  ${CMAKE_CURRENT_SOURCE_DIR}/brightmaps/weapons/*
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    ${bdw_pak_SRC}
  )
add_custom_target(bdw_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/mods/bdw)
