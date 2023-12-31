configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

file(GLOB basepak_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/credits/*.txt

  ${CMAKE_CURRENT_SOURCE_DIR}/actors/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/chex/*
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/doom/*
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/heretic/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/hexen/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/k8gore/*
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/k8tracers/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/raven/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/shared/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/actors/strife/*.txt

  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/common/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/doom/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/heretic/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/hexen/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/k8tracers/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/raven/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/fxdefs/strife/*.txt

  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/*.lmp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/progbar/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/healthbar/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/screenback/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/k8goreBloodDecals/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/k8vavoom_special/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/msgbox/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics/ui/*.png

  ${CMAKE_CURRENT_SOURCE_DIR}/sounds/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sounds/k8gore/*

  ${CMAKE_CURRENT_SOURCE_DIR}/models/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/k8lightbulb/*
  ${CMAKE_CURRENT_SOURCE_DIR}/models/k8tracer/*

  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/*.png
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/dogs/*
  ${CMAKE_CURRENT_SOURCE_DIR}/sprites/gore/*

  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/menus/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/menus/debug/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/menus/gmopts/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/menus/sound/*.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/uidef/menus/video/*.txt

  ${CMAKE_SOURCE_DIR}/progs/common/botai/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/engine/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/engine/vccrun/*.vc

  ${CMAKE_SOURCE_DIR}/progs/common/uibase/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/client/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/client/hudmsg/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/condlg/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/finale/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/menus/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgchoice/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgselector/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgslotsl/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgtextbtn/*.vc

  ${CMAKE_SOURCE_DIR}/progs/common/linespec/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/actor/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/armor/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/basic/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/bridge/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/camera/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/dynlight/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/entityex/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/heretic/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/hexen/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/raven/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/strife/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/games/strife/health/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/health/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/incomplete/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/linespec/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/particle/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/player/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/polythink/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/portals/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/powerup/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/secaction/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/lights/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/movers/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/sound/*.vc
  ${CMAKE_SOURCE_DIR}/progs/common/linespec/weapon/*.vc

  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/common/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/common/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/common/*.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/decal/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/decal/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/draw/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/draw/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/lightmap/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/lightmap/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/masked/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/masked/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/masked/poly/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/masked/poly/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/particle/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/particle/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/model/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/model/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/surf/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/shadowvol/surf/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/sky/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/sky/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/wipe/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/wipe/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/bloom/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/bloom/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/cas/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/cas/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/colorblind/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/colorblind/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/colormatrix/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/colormatrix/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/overbright/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/overbright/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/tonemap/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/tonemap/*.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/underwater/*.fs
  ${CMAKE_CURRENT_SOURCE_DIR}/glshaders/postprocess/underwater/*.vs
  )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS --vwad --keyfile ${CMAKE_SOURCE_DIR}/k8_ed25519_priv.key basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
  ${CMAKE_CURRENT_SOURCE_DIR}/basepak.ls.in
  ${CMAKE_CURRENT_SOURCE_DIR}/confont.pcx
  ${CMAKE_CURRENT_SOURCE_DIR}/dbigfont.pcx
  ${CMAKE_CURRENT_SOURCE_DIR}/k8vavoom_default.cfg
  ${CMAKE_CURRENT_SOURCE_DIR}/language.en
  ${CMAKE_CURRENT_SOURCE_DIR}/k8vavoom_startup.vs
  ${CMAKE_CURRENT_SOURCE_DIR}/vavoom_decorate_defs.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/vavoom_known_blood.rc
  ${CMAKE_CURRENT_SOURCE_DIR}/vavoom_class_ignores.rc
  ${CMAKE_CURRENT_SOURCE_DIR}/gamecontrollerdb.txt

  ${CMAKE_CURRENT_SOURCE_DIR}/textures/-noflat-.png

  ${basepak_SRC}
  )

add_custom_target(common_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.vwad
  DESTINATION ${DATADIR}/basev/common)
