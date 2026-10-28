cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0177)
    # Policy CMP0177: install() DESTINATION paths are normalized
    # Introduced in CMake 3.31
    cmake_policy(SET CMP0177 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(identity)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_CLIENT "Build client" ON)
option(BUILD_RENDERER_GL1 "Build GL1 renderer" ON)
option(BUILD_RENDERER_GL2 "Build GL2 renderer" OFF)
option(BUILD_GAME_LIBRARIES "Build game module libraries" ON)
option(BUILD_GAME_QVMS "Build game module qvms" ON)
option(BUILD_STANDALONE "Build binaries for standalone games" OFF)

option(USE_RENDERER_DLOPEN "Dynamically load the renderer(s)" ON)
option(USE_OPENAL "OpenAL audio" ON)
option(USE_OPENAL_DLOPEN "Dynamically load OpenAL" ON)
option(USE_HTTP "HTTP download support" ON)
option(USE_CODEC_VORBIS "Ogg Vorbis support" ON)
option(USE_CODEC_OPUS "Ogg Opus support" ON)
option(USE_CODEC_MAD "MP3 support" ON)
option(USE_VOIP "Voice chat" OFF)
option(USE_MUMBLE "Mumble support" OFF)
option(USE_FREETYPE "Freetype font rendering" OFF)

option(USE_INTERNAL_LIBS "Use internally packaged libraries" ON)
option(USE_INTERNAL_SDL "Use internal SDL binary (if available)" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_ZLIB "Use internal copy of zlib" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_JPEG "Use internal copy of libjpeg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_OGG "Use internal copy of ogg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_VORBIS "Use internal copy of vorbis" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_OPUS "Use internal copy of opus" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_MAD "Use internal copy of libmad" ${USE_INTERNAL_LIBS})

# Release build by default, set externally if you want something else
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Try to enable LTO by default
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/code)

# Enable unit testing
include(CTest)

if(DEFINED ENV{SOURCE_DATE_EPOCH})
    string(TIMESTAMP PRODUCT_DATE "%b %d %Y" UTC)
    add_compile_definitions(PRODUCT_DATE="${PRODUCT_DATE}")
endif()

include(compilers/all)
include(platforms/all)
include(libraries/all)
include(deploy/all)

include(version)

include(server)
include(renderer_gl1)
include(renderer_gl2)
include(client)
include(basegame)
#include(missionpack)
include(launcher)

include(post_configure)
#include(installer)

# NXDK_STATIC_GAME: game linkado estatico no XBE
if(NXDK AND TARGET omohaaded)
  get_target_property(_lg_game_tgt omohaaded NAME)
  foreach(_g game fgame omohaaded_game)
    if(TARGET ${_g})
      set_property(TARGET omohaaded APPEND PROPERTY LINK_LIBRARIES ${_g})
      message(STATUS "NXDK_STATIC_GAME: omohaaded <- ${_g}")
      break()
    endif()
  endforeach()
endif()

# NXDK_FORCE_MULTIPLE: corepp existe no engine e na libgame
if(NXDK AND TARGET omohaaded)
  # LG 16z9e: target_link_options(omohaaded PRIVATE "/FORCE:MULTIPLE")
endif()

# LG lote 15i: isolamento ODR engine x game (nxdk)
if(TARGET game)
  target_compile_options(game PRIVATE "-include${CMAKE_SOURCE_DIR}/tools/nxdk-shims/lg_game_rename.h")
endif()

# LG18h: game linkado estatico no cliente nxdk
if(NXDK AND TARGET openmohaa AND TARGET game)
    target_link_libraries(openmohaa PRIVATE game)
endif()
# LG18z6b: cgame linkado estatico no cliente nxdk (rename proprio)
if(NXDK AND TARGET openmohaa AND TARGET cgame)
  target_compile_options(cgame PRIVATE "-include${CMAKE_SOURCE_DIR}/tools/nxdk-shims/lg_cgame_rename.h")
  target_link_libraries(openmohaa PRIVATE cgame)
endif()
# LG18z6c: stub GetCGameAPI no dedicado (sys_main_new.c compartilhado)
if(NXDK AND TARGET omohaaded)
  target_sources(omohaaded PRIVATE ${CMAKE_SOURCE_DIR}/tools/nxdk-shims/lg_cgame_stub_ded.c)
endif()
