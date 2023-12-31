if(WITH_GLES)
  set(VSHADPP_DEFINES -DGL4ES_HACKS)
endif()

function(sxx_compile output filename)
  get_filename_component(ext ${filename} EXT)
  if(NOT ext STREQUAL ".sxx")
    return()
  endif()
  get_filename_component(base ${filename} NAME_WE)
  set(base_abs ${CMAKE_CURRENT_BINARY_DIR}/${base})
  set(outfiles ${base_abs}.ci ${base_abs}.hi)
  set(${output} ${${output}} ${outfiles} PARENT_SCOPE)
  #message("outfiles=${outfiles}, DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
  if(CMAKE_CROSSCOMPILING AND NOT WIN32)
    file(GLOB vshadpp_FILES CONFIGURE_DEPENDS
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.inc
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/decal/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/decal/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/draw/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/draw/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/lightmap/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/lightmap/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/poly/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/poly/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/particle/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/particle/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/model/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/model/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/surf/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/surf/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/sky/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/sky/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/wipe/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/wipe/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/bloom/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/bloom/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/cas/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/cas/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colorblind/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colorblind/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colormatrix/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colormatrix/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/overbright/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/overbright/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/tonemap/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/tonemap/*.vs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/underwater/*.fs
      ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/underwater/*.vs
      )
    # assume we have vshadpp in PATH
    add_custom_command(
      OUTPUT ${outfiles}
      COMMAND vshadpp ${VSHADPP_DEFINES} -o ${CMAKE_CURRENT_BINARY_DIR} -b ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      ${vshadpp_FILES}
      #${CMAKE_CURRENT_SOURCE_DIR}/${mastername}
    )
else()
  file(GLOB vshadpp_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.inc
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/decal/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/decal/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/draw/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/draw/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/lightmap/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/lightmap/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/poly/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/masked/poly/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/particle/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/particle/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/model/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/model/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/surf/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/shadowvol/surf/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/sky/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/sky/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/wipe/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/wipe/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/bloom/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/bloom/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/cas/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/cas/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colorblind/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colorblind/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colormatrix/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/colormatrix/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/overbright/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/overbright/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/tonemap/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/tonemap/*.vs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/underwater/*.fs
    ${CMAKE_SOURCE_DIR}/basev/common/glshaders/postprocess/underwater/*.vs
    )

    add_custom_command(
      OUTPUT ${outfiles}
      COMMAND ${WINERUN} $<TARGET_FILE:vshadpp> ${VSHADPP_DEFINES} -o ${CMAKE_CURRENT_BINARY_DIR} -b ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      DEPENDS vshadpp ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      ${vshadpp_FILES}
      #${CMAKE_CURRENT_SOURCE_DIR}/${mastername}
    )
  endif()
  set_source_files_properties(${outfiles} PROPERTIES GENERATED TRUE)
endfunction()


if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DVAVOOM_DEBUG_BUILD=1)
endif()



#---------------------------------------
#
# Texture loader and manager sources
#
#---------------------------------------
set(TEXTURELIB_SOURCES
  textures/r_tex_id.h
  textures/r_tex_public.h
  textures/r_tex.h
  textures/r_tex.cpp
  textures/r_tex_id.cpp
  textures/r_tex_animdef.cpp
  textures/r_tex_base.cpp
  textures/r_tex_camera.cpp
  textures/r_tex_warp.cpp
  textures/r_tex_translation.cpp
  textures/r_tex_atlas.cpp
  # image loaders
  textures/formats/img_automap.cpp
  textures/formats/img_flat.cpp
  textures/formats/img_imgz.cpp
  textures/formats/img_jpeg.cpp
  textures/formats/img_multipatch.cpp
  textures/formats/img_patch.cpp
  textures/formats/img_pcx.cpp
  textures/formats/img_png.cpp
  textures/formats/img_raw.cpp
  textures/formats/img_tga.cpp
)

set(RENDERBASE_SOURCES
  render/r_local.h
  render/r_public.h
  render/r_shared.h
  render/r_bsp.cpp
  render/r_bsp_reg.cpp
  render/r_bsp_adv.cpp
  render/r_data.cpp
  render/r_data.cpp
  render/r_light.cpp
  render/r_light_reg.cpp
  render/r_light_reg_check.cpp
  render/r_light_adv.cpp
  render/r_light_adv_collect.cpp
  render/r_light_adv_cvars.cpp
  render/r_light_adv_fltopt.cpp
  render/r_main.cpp
  render/r_main_base.cpp
  render/r_main_decals.cpp
  render/r_main_reg.cpp
  render/r_main_adv.cpp
  render/r_model_parse.cpp
  render/r_model_gz.cpp
  render/r_model_render.cpp
  render/modelparse/md2_normals.h
  render/modelparse/voxelib.cpp
  render/modelparse/r_parsecommon.cpp
  render/modelparse/r_parse_md2.cpp
  render/modelparse/r_parse_md3.cpp
  render/modelparse/r_parse_kvx.cpp
  render/r_particle.cpp
  render/r_portal.cpp
  render/r_sky.cpp
  render/r_surf_1sided.cpp
  render/r_surf_2sextra.cpp
  render/r_surf_2sided.cpp
  render/r_surf_2sided_pobj.cpp
  render/r_surf_axes_texture.cpp
  render/r_surf_common.cpp
  render/r_surf_flat.cpp
  render/r_surf_lmap_io.cpp
  render/r_surf_mgmt.cpp
  render/r_surf_opening.cpp
  render/r_surf_post_adv.cpp
  render/r_surf_post_reg.cpp
  render/r_surf_tjunk.cpp
  render/r_surf_update.cpp
  render/r_surf_update_fake.cpp
  render/r_utils_openings.cpp
  render/r_maskqueue.cpp
  render/r_psprites.cpp
  render/r_things.cpp
  render/r_things_adv.cpp
  render/r_update.cpp
  render/r_vis_flood.cpp
  render/r_vis_bsp.cpp
  render/r_vis_light.cpp
)

#---------------------------------------
#
# sound subsystem sources
#
#---------------------------------------
set(SOUNDSYS_SOURCES
  sound/sound.h
  sound/snd_local.h
  sound/snd_data.cpp
  sound/snd_main.cpp

  sound/drv/snd_al.cpp

  sound/utils/snd_midisong.cpp
  sound/utils/snd_qmus2mid.cpp
  sound/utils/snd_sf2scan.cpp
  #k8:not used, 'cause there is no EAX/EFX support: sound/utils/snd_reverbs.cpp

  sound/snd_streamplayer.cpp

  sound/nukedopl/opl3.c
  sound/nukedopl/musmid.cpp

  sound/decoders/snd_timidity.cpp
  sound/decoders/snd_wav.cpp
  sound/decoders/snd_xmplite.cpp
  sound/decoders/snd_nuked.cpp
)

if(WITH_MOJOAL)
  set(SOUNDSYS_SOURCES ${SOUNDSYS_SOURCES} sound/mojoal/mojoal.c)
endif(WITH_MOJOAL)


#---------------------------------------
#
# network subsystem sources
#
#---------------------------------------
set(NETSUB_SOURCES
  net/network.h
  net/net_channel.cpp
  net/net_channel_control.cpp
  net/net_channel_level.cpp
  net/net_channel_object_map.cpp
  net/net_channel_player.cpp
  net/net_channel_thinker.cpp
  net/net_connection.cpp
  net/net_context.cpp
  net/net_datagram.cpp
  net/net_demo.cpp
  net/net_local.h
  net/net_loopback.cpp
  net/net_main.cpp
  net/net_message.cpp
  net/net_message.h
  net/net_object_map.cpp
  net/net_udp.cpp
)


#---------------------------------------
#
# ajbsp sources
#
#---------------------------------------
set(AJBSP_SOURCES
  bsp/ajbsp/bsp_level.cpp
  bsp/ajbsp/bsp_node.cpp
  bsp/ajbsp/bsp_util.cpp
  #ajbsp/lib_util.cpp
)


#---------------------------------------
#
# zdbsp sources
#
#---------------------------------------
set(ZDBSP_SOURCES
  bsp/zdbsp/nodebuild.cpp
  bsp/zdbsp/nodebuild_events.cpp
  bsp/zdbsp/nodebuild_extract.cpp
  bsp/zdbsp/nodebuild_gl.cpp
  bsp/zdbsp/nodebuild_utility.cpp
  bsp/zdbsp/nodebuild_classify_nosse2.cpp
  bsp/zdbsp/zdbsp_extr.cpp
)


#---------------------------------------
#
# file subsystem sources
#
#---------------------------------------
set(FILESYS_SOURCES
  filesys/files.cpp
  filesys/files.h
)


set(VLEVEL_CLASS_SOURCES
  level/level.h
  level/level.cpp
  level/level_3dcoldet.cpp
  level/level_3dfloors.cpp
  level/level_3dpobj_coldet.cpp
  level/level_calclineseg.cpp
  level/level_dbgexport.cpp
  level/level_decals.cpp
  level/level_decals_spread_flat.cpp
  level/level_decals_spread_wall.cpp
  level/level_decals_bootcheck.cpp
  level/level_light.cpp
  level/level_miscapi.cpp
  level/level_nodebbox.cpp
  level/level_opening.cpp
  level/level_saveio.cpp
  level/level_secnode.cpp
  level/level_sector_change.cpp
  level/level_soundprop.cpp
  level/level_think.cpp
  level/beamclip.cpp
  level/beamclip.h
)

set(SV_SOURCES
  server/server.h
  server/sv_save.h
  server/sv_local.h
  server/sv_main.cpp
  server/sv_save.cpp
)

set(MAPLOADER_SOURCES
  maploader/mapload.cpp
  maploader/mapload_cacheio.cpp
  maploader/mapload_udmf.cpp
  maploader/loader/lines.cpp
  maploader/loader/sectors.cpp
  maploader/loader/verts.cpp
  maploader/loader/sides.cpp
  maploader/loader/reject.cpp
  maploader/loader/things.cpp
  maploader/loader/acs.cpp
  maploader/mapload_calc.cpp
  maploader/mapload_decals.cpp
  maploader/mapload_nodes_aj.cpp
  maploader/mapload_nodes_zd.cpp
  maploader/mapload_fixers.cpp
  maploader/mapload_postload.cpp
  maploader/mapload_blockmap.cpp
)

set(ACS_SOURCES
  acs/p_acs.h
  acs/p_acs_extfunc.h
  acs/p_acs.cpp
)

set(PSIM_SOURCES
  psim/p_decal.h
  psim/p_decal_private.h
  psim/p_decal.cpp
  psim/p_entity.h
  psim/p_entity.cpp
  psim/p_entity_misc.cpp
  psim/p_entity_sight.cpp
  #psim/p_entity_world.cpp
  psim/p_entity_checkpos.cpp
  psim/p_entity_checkrelpos.cpp
  psim/p_entity_worldlink.cpp
  psim/p_entity_worldtrymove.cpp
  psim/p_entity_worldslide.cpp
  psim/p_entity_worldbounce.cpp
  psim/p_gameinfo.cpp
  psim/p_gameinfo.h
  psim/p_gameobject.cpp
  psim/p_gameobject.h
  psim/p_levelinfo.cpp
  psim/p_levelinfo.h
  psim/p_player.cpp
  psim/p_player.h
  psim/p_player_input.cpp
  psim/p_playerreplicationinfo.cpp
  psim/p_playerreplicationinfo.h
  psim/p_polyobj_misc.cpp
  psim/p_polyobj_movement.cpp
  psim/p_polyobj_spawn.cpp
  psim/p_polyobj_vcapi.cpp
  psim/p_switch.cpp
  psim/p_terrain.cpp
  psim/p_thinker.cpp
  psim/p_thinker.h
  psim/p_trace_bsp.cpp
  #psim/p_trace_sight.cpp
  #psim/p_trace_light.cpp
  psim/p_trace_sightlight.cpp
  psim/p_world.cpp
  psim/p_world.h
  psim/p_worldinfo.cpp
  psim/p_worldinfo.h
)


#---------------------------------------
#
# VavoomC compiler support sources
#
#---------------------------------------
set(VCCLIB_SOURCES
  decorate/vc_decorate.h
  decorate/vc_decorate.cpp
  dehacked/vc_dehacked.h
  dehacked/vc_dehacked.cpp
  vcapi/api_vavoom.cpp
  vcapi/pr_cmds.cpp
)


#---------------------------------------
#
# Main executable sources
#
#---------------------------------------
#file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h)
#file(TOUCH ${CMAKE_CURRENT_SOURCE_DIR}/gitversion.h.in)
##configure_file(
##  gitversion.h.in
##  gitversion.h
##)
### ### if(FOSSIL_REPO_PATH)
### ###   execute_process(
### ###     COMMAND fossil time -R "${FOSSIL_REPO_PATH}" -n 1 -F "%H" -t ci
### ###     COMMAND head -c 16
### ###     DEPENDS host.cpp
### ###     RESULT_VARIABLE
### ###       FOSSIL_HASH_RESULT
### ###     OUTPUT_VARIABLE
### ###       FOSSIL_HASH)
### ###
### ###   message(STATUS "Fossil commit hash: " ${FOSSIL_HASH})
### ###   add_definitions("-DFOSSL_COMMIT_HASH=${FOSSIL_HASH}")
### ### endif(FOSSIL_REPO_PATH)

if(FOSSIL_REPO_PATH)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fossilversion.h
    COMMAND sh ${CMAKE_SOURCE_DIR}/utils/getfossver.sh "${FOSSIL_REPO_PATH}" ${CMAKE_CURRENT_BINARY_DIR}/fossilversion.h
    DEPENDS host.cpp
  )
  add_definitions("-DHAVE_FOSSIL_COMMIT_HASH")
  set(SRC_FOSSVER "${CMAKE_CURRENT_BINARY_DIR}/fossilversion.h")
else(FOSSIL_REPO_PATH)
  set(SRC_FOSSVER )
endif(FOSSIL_REPO_PATH)

#---------------------------------------
#
# Sources shared between cliend and server
#
#---------------------------------------
set(CS_SHARED_SOURCES
  #gitversion.h.in
  gamedefs_build.h
  gamedefs_fwd.h
  gamedefs.h
  cheats.cpp
  cmd.h
  cmd.cpp
  cvar.h
  cvar.cpp

  # yeah, the server needs this
  text.h
  text.cpp

  host.h
  host.cpp
  infostr.h
  infostr.cpp
  language.h
  language.cpp
  lockdefs.h
  lockdefs.cpp

  mapinfo.h
  mapinfo.cpp

  utils/misc.h
  utils/misc.cpp
  utils/qs_data.h
  utils/qs_data.cpp
  utils/ntvalueioex.h
  utils/ntvalueioex.cpp

  utils/scripts.h
  utils/scripts.cpp

  ${FILESYS_SOURCES}
  ${VLEVEL_CLASS_SOURCES}
  ${NETSUB_SOURCES}
  ${ACS_SOURCES}
  ${PSIM_SOURCES}
  ${MAPLOADER_SOURCES}
  # yeah, server loads textures too
  ${TEXTURELIB_SOURCES}

  ${VCCLIB_SOURCES}

  ${AJBSP_SOURCES}
  ${ZDBSP_SOURCES}

  ${SV_SOURCES}
)


#-- source files for combined client/server --
set(COMBINED_SOURCES
  #${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
  #${CMAKE_CURRENT_BINARY_DIR}/fossilversion.h
  ${SRC_FOSSVER}
  ${CS_SHARED_SOURCES}

  ${RENDERBASE_SOURCES}
  ${SOUNDSYS_SOURCES}

  client/client.h
  client/cl_local.h
  client/cl_main.cpp

  widgets/ui.h
  widgets/ui_actor.cpp
  widgets/ui_font.cpp
  widgets/ui_font.h
  widgets/ui_root.cpp
  widgets/ui_root.h
  widgets/ui_widget.cpp
  widgets/ui_widget.h

  automap.h
  automap.cpp
  chat.h
  chat.cpp
  console.cpp
  console.h
  drawer.h
  drawer.cpp
  finale.h
  finale.cpp
  iline.h
  iline.cpp
  in_input.cpp
  input.h
  menu.h
  menu.cpp
  sbar.h
  sbar.cpp
  screen.h
  screen.cpp
  touch.cpp
  touch.h
  video.h
)

#-- Common OpenGL files --
set(MAIN_OPENGL_SOURCES
  render/hwgl/gl_local.h
  render/hwgl/gl_bloom.cpp
  render/hwgl/gl_decal.cpp
  render/hwgl/gl_draw.cpp
  render/hwgl/gl_main.cpp
  render/hwgl/gl_main_ctl.cpp
  render/hwgl/gl_main_cubemap.cpp
  render/hwgl/gl_main_fbo.cpp
  render/hwgl/gl_main_postprocess.cpp
  render/hwgl/gl_main_scissor.cpp
  render/hwgl/gl_main_shader.cpp
  render/hwgl/gl_main_views.cpp
  render/hwgl/gl_main_wipe.cpp
  render/hwgl/gl_masked.cpp
  render/hwgl/gl_model.cpp
  render/hwgl/gl_particle.cpp
  render/hwgl/gl_poly.cpp
  # advanced redenrer
  render/hwgl/gl_poly_adv_render.h
  render/hwgl/gl_poly_adv.cpp
  render/hwgl/gl_poly_adv_render_ambient.cpp
  render/hwgl/gl_poly_adv_render_fog.cpp
  render/hwgl/gl_poly_adv_render_light.cpp
  render/hwgl/gl_poly_adv_render_pre.cpp
  render/hwgl/gl_poly_adv_render_smap.cpp
  render/hwgl/gl_poly_adv_render_svol.cpp
  render/hwgl/gl_poly_adv_render_textures.cpp
  render/hwgl/gl_poly_adv_shadowopt.cpp
  render/hwgl/gl_poly_adv_zpass.cpp
  # end of advanced renderer
  render/hwgl/gl_portal.cpp
  render/hwgl/gl_tex.cpp
  render/hwgl/gl_shaddef.sxx
  ${CMAKE_CURRENT_BINARY_DIR}/gl_shaddef.hi
  ${CMAKE_CURRENT_BINARY_DIR}/gl_shaddef.ci
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
foreach(file ${MAIN_OPENGL_SOURCES})
  sxx_compile(OPENGL_GENERATED_SOURCES ${file})
endforeach()

source_group("" FILES ${MAIN_OPENGL_SOURCES})
#source_group(generated FILES ${OPENGL_GENERATED_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h)
if(FOSSIL_REPO_PATH)
  source_group(generated FILES ${OPENGL_GENERATED_SOURCES} ${SRC_FOSSVER})
endif(FOSSIL_REPO_PATH)

#add_executable(vshadpp ${MAIN_OPENGL_SOURCES} ${OPENGL_GENERATED_SOURCES})
#sxx_compile(render/hwgl/gl_shaddef.sxx  render/hwgl/gl_main.cpp)


include_directories( .
  vc
  ../libs
  ${SYSTEM_SOURCES_DIR}
)

#---------------------------------------
#
# Dedicated server sources
#
#---------------------------------------
set(SERVER_COMMON_SOURCES
  #${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
  #${CMAKE_CURRENT_BINARY_DIR}/fossilversion.h
  ${SRC_FOSSVER}
  ${CS_SHARED_SOURCES}

  # for loading various definitions
  sound/snd_data.cpp
  # for skybox parsing
  render/r_sky.cpp
  # for loading various definitions
  render/r_data.cpp
)


#---------------------------------------
#
# svnrev.h stuff
#
#---------------------------------------

### add_custom_target( revision_check ALL
###   COMMAND updaterevision ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/svnrev.h
###   WORKING_DIRECTORY ${UPDATEREVISION_EXE}
###   DEPENDS updaterevision )

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(NOT WIN32)
  set(K8VAVOOMDEV OFF)
  if("$ENV{K8VAVOOMDEV}" STREQUAL "tan")
    set(K8VAVOOMDEV ON)
  endif()
  if("$ENV{USER}" STREQUAL "ketmar")
    set(K8VAVOOMDEV ON)
  endif()

  if(K8VAVOOMDEV)
    add_definitions(-DVAVOOM_K8_DEVELOPER)
  endif(K8VAVOOMDEV)
endif(NOT WIN32)


#---------------------------------------
#
# Main executable
#
#---------------------------------------
if(ENABLE_CLIENT)
  set(MAIN_SOURCES ${COMBINED_SOURCES})
  #set(MAIN_LIBS glbsp libglvis timidity vavoomc core ${VAVOOM_MAIN_DEPS} ${NET_LIBRARIES} ${MAIN_LIBS})
  set(MAIN_LIBS timidity vavoomc core ${VAVOOM_MAIN_DEPS} ${NET_LIBRARIES} ${MAIN_LIBS})
  set(MAIN_COMPILE_FLAGS "")

  if(VORBIS_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_vorbis.cpp)
  else(VORBIS_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_vorbis_stb.cpp)
  endif(VORBIS_FOUND)

  #k8: we have internal opus library
  if(OPUS_FOUND OR OPUS_USE_INTERNAL)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_opus.cpp)
  endif(OPUS_FOUND OR OPUS_USE_INTERNAL)

  if(LIBMAD_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_mp3_mad.cpp)
  else(LIBMAD_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_mp3_dr.cpp)
  endif(LIBMAD_FOUND)

  if(FLAC_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_flac.cpp)
  else(FLAC_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_flac_dr.cpp)
  endif(FLAC_FOUND)

  if(FLUID_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/decoders/snd_fluid.cpp)
  endif(FLUID_FOUND)


  # shitdoze icon
  if(WIN32)
    #  CMake doesn't support automatic building of resource files so
    # we must add a custom command and add object files to the list
    # of source files
    set(RC_SRC ${CMAKE_SOURCE_DIR}/branding/shitdoze.rc)
    set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/shitdoze.o)
    add_custom_command(OUTPUT ${RC_OBJ}
      DEPENDS ${RC_SRC}
      COMMAND windres -o ${RC_OBJ} -i ${RC_SRC} ${RES_FLAGS}
    )
    set(MAIN_SOURCES ${MAIN_SOURCES} ${RC_OBJ})
  endif(WIN32)


  if(SDL2_FOUND)
    if(WIN32)
      set(MAIN_LIBS gdi32 ole32 winmm imm32 version ${MAIN_LIBS})
    endif(WIN32)
    set(MAIN_SOURCES ${MAIN_SOURCES} in_sdl.cpp sys_sdl.cpp)
    set(MAIN_SOURCES ${MAIN_SOURCES} render/hwgl/gl_sdl.cpp render/hwgl/gl_dpi.cpp)
    ###set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} imago)
    ###set(MAIN_LIBS imago ${MAIN_LIBS})
  endif(SDL2_FOUND)


  ### if(OPENGL_FOUND)
  ###   set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
  ###   if(WIN32)
  ###     set(MAIN_SOURCES ${MAIN_SOURCES} winshit/render/hwgl/gl_win32.cpp)
  ###     set(MAIN_LIBS opengl32 ${MAIN_LIBS})
  ###   else(WIN32)
  ###     set(MAIN_SOURCES ${MAIN_SOURCES} render/hwgl/gl_sdl.cpp)
  ###   endif(WIN32)
  ### endif(OPENGL_FOUND)
  if(OPENGL_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
  endif(OPENGL_FOUND)


  if(CYGWIN OR MINGW)
    # prune --out-implib from executables: it doesn't make sense, even with --export-all-symbols
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  endif()

  if(ANDROID)
    add_library(k8vavoom SHARED ${MAIN_SOURCES})
  else()
    add_executable(k8vavoom ${MAIN_SOURCES})
    if(ENABLE_WRAPPERS)
      set_target_properties(k8vavoom PROPERTIES OUTPUT_NAME ../k8vavoom.bin)
    else(ENABLE_WRAPPERS)
      set_target_properties(k8vavoom PROPERTIES OUTPUT_NAME ../k8vavoom)
    endif(ENABLE_WRAPPERS)
  endif()
  if (MAIN_COMPILE_FLAGS)
    set_target_properties(k8vavoom PROPERTIES COMPILE_FLAGS ${MAIN_COMPILE_FLAGS})
  endif (MAIN_COMPILE_FLAGS)
  #message(STATUS "MAIN LINKLIBS: " ${MAIN_LIBS})
  target_link_libraries(k8vavoom ${MAIN_LIBS})
  #add_dependencies(k8vavoom core vavoomc timidity glbsp libglvis ${VAVOOM_MAIN_DEPS})
  add_dependencies(k8vavoom core vavoomc timidity ${VAVOOM_MAIN_DEPS})

  install(TARGETS k8vavoom DESTINATION ${BINDIR})

  #
  # Icon
  #
  install(FILES ${CMAKE_SOURCE_DIR}/docs/k8vavoom.txt DESTINATION ${DATADIR})
  install(FILES ${CMAKE_SOURCE_DIR}/res/k8vavoom.png DESTINATION ${DATADIR})
  # more docs
  install(FILES ${CMAKE_SOURCE_DIR}/docs/code_of_conduct.txt DESTINATION ${DATADIR}/docs)
  install(FILES ${CMAKE_SOURCE_DIR}/README DESTINATION ${DATADIR}/docs)
  # variants
  if(WIN32)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon2.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon3.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon4.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon5.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon6.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon7.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon8.ico DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/ico/k8vavoom_icon9.ico DESTINATION ${DATADIR}/icons)
  else(WIN32)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon2.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon3.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon4.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon5.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon6.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon7.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon8.png DESTINATION ${DATADIR}/icons)
    install(FILES ${CMAKE_SOURCE_DIR}/res/png/k8vavoom_icon9.png DESTINATION ${DATADIR}/icons)
  endif(WIN32)

  # some specs and demos
  if(WIN32)
    set(DSPECSDIR ${SPECSDIR})
  else(WIN32)
    set(DSPECSDIR ${DATADIR}/${SPECSDIR})
  endif(WIN32)
  #
  #install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/k8vavoom_3dpobj.html DESTINATION ${DSPECSDIR}/3dpobj)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/k8vavoom_3dpobj.wiki DESTINATION ${DSPECSDIR}/3dpobj)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/fig1.png DESTINATION ${DSPECSDIR}/3dpobj)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/zacc_acs/zdefs.acs DESTINATION ${DSPECSDIR}/3dpobj/zacc_acs)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/zacc_acs/zspecial.acs DESTINATION ${DSPECSDIR}/3dpobj/zacc_acs)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_crush.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_crush_side.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_railings.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_railings_blockhitscan.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_railings_blockproj.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/3d_polyobject_cabin_simple.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/big_dropoff.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/big_sight_faraway.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/empty.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/rotate.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/rotate_empty.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/simple_test.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/boris/3dpolyobj_testmap00.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads/boris)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/complex/3d_polyobject_cabin_move_spot.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads/complex)
  install(FILES ${CMAKE_SOURCE_DIR}/specs/3dpobj/sample_wads/complex/3d_polyobject_cabin_move_spot_doors.wad DESTINATION ${DSPECSDIR}/3dpobj/sample_wads/complex)
  # more specs
  install(FILES ${CMAKE_SOURCE_DIR}/specs/index.wiki DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/udmf_k8vavoom.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/k8vavoom_terrain_bootprints.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/k8vavoom_pointlights.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/k8vavoom_decals.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/k8vavoom_decorate.txt DESTINATION ${DSPECSDIR})
  # unfinished specs
  install(FILES ${CMAKE_SOURCE_DIR}/specs/unfinished/k8vavoom_models.txt DESTINATION ${DSPECSDIR}/unfinished)
  # non-specific specs
  install(FILES ${CMAKE_SOURCE_DIR}/specs/umapinfo.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/udmf.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/udmf_zdoom.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/usdf.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/usdf_zdoom.txt DESTINATION ${DSPECSDIR})
  install(FILES ${CMAKE_SOURCE_DIR}/specs/usdf_gzdoom.txt DESTINATION ${DSPECSDIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/k8vavoom.bin\" \"\$\@\" -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)

  if(NSWITCH)
    add_custom_command(
      TARGET k8vavoom POST_BUILD
      COMMAND nacptool --create "${CMAKE_PROJECT_NAME}" \"\" \"\" "k8vavoom.nacp"
      COMMAND aarch64-none-elf-strip -o "${CMAKE_BINARY_DIR}/k8vavoom_stripped" "${CMAKE_BINARY_DIR}/k8vavoom"
      COMMAND elf2nro "${CMAKE_BINARY_DIR}/k8vavoom_stripped" "${CMAKE_BINARY_DIR}/k8vavoom.nro" --icon="${CMAKE_SOURCE_DIR}/branding/k8vavoom_switch.jpg" --nacp=k8vavoom.nacp
    )
  endif(NSWITCH)
endif(ENABLE_CLIENT)


#---------------------------------------
#
# Dedicated server
#
#---------------------------------------
if(ENABLE_SERVER)
  ### if(WIN32)
  ###   set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} winshit/sys_wind.cpp)
  ###
  ###   #  CMake doesn't support automatic building of resource files so
  ###   # we must add a custom command and add object files to the list
  ###   # of source files
  ###   ### set(RC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/winshit/res/k8vavoom.rc)
  ###   ### set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/winshit/res/k8vavoom_sv.o)
  ###   ### add_custom_command(OUTPUT ${RC_OBJ}
  ###   ###   DEPENDS ${RC_SRC}
  ###   ###   COMMAND windres --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_BINARY_DIR} -o ${RC_OBJ} -i ${RC_SRC}
  ###   ### )
  ###   ### set(SERVER_SOURCES ${SERVER_SOURCES} ${RC_OBJ})
  ###   #set(SERVER_SOURCES ${SERVER_SOURCES})
  ### else(WIN32)
  ###   set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} sys_ded.cpp)
  ### endif(WIN32)
  set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} sys_ded.cpp)

  add_executable(k8vavoom-dedicated ${SERVER_SOURCES})
  if(CYGWIN OR MINGW)
    set_target_properties(k8vavoom-dedicated PROPERTIES LINK_FLAGS "-Wl,--subsystem,console")
  endif()

  if(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom-dedicated PROPERTIES OUTPUT_NAME ../k8vavoom-dedicated.bin)
  else(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom-dedicated PROPERTIES OUTPUT_NAME ../k8vavoom-dedicated)
  endif(ENABLE_WRAPPERS)
  set_target_properties(k8vavoom-dedicated PROPERTIES COMPILE_FLAGS -DSERVER)
  #FIXME: create separate from MAIN_LIBS link list
  #target_link_libraries(k8vavoom-dedicated glbsp vavoomc core ${MAIN_LIBS} ${NET_LIBRARIES})
  target_link_libraries(k8vavoom-dedicated vavoomc core ${MAIN_LIBS} ${NET_LIBRARIES})
  #add_dependencies(k8vavoom-dedicated core vavoomc glbsp ${VAVOOM_MAIN_DEPS})
  add_dependencies(k8vavoom-dedicated core vavoomc ${VAVOOM_MAIN_DEPS})

  install(TARGETS k8vavoom-dedicated DESTINATION ${BINDIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom-dedicated
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/k8vavoom-dedicated.bin\" \"\$\@\" -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom-dedicated DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)
endif(ENABLE_SERVER)
