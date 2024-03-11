cmake_minimum_required(VERSION 3.22...3.25)

option(ENABLE_NVENC "Build NVIDIA Hardware Encoder Plugin" ON)
if(NOT ENABLE_NVENC)
  target_disable_feature(obs-nvenc "NVIDIA Hardware Encoder")
  target_disable(obs-nvenc)
  return()
endif()

include(cmake/dependencies.cmake)

add_library(obs-nvenc MODULE)
add_library(OBS::nvenc ALIAS obs-nvenc)

add_subdirectory(obs-nvenc-test)

target_sources(
  obs-nvenc
  PRIVATE # cmake-format: sortable
          $<$<PLATFORM_ID:Linux>:nvenc-opengl.c>
          $<$<PLATFORM_ID:Windows>:nvenc-d3d11.c>
          cuda-helpers.c
          cuda-helpers.h
          nvenc-compat.c
          nvenc-cuda.c
          nvenc-helpers.c
          nvenc-helpers.h
          nvenc-internal.h
          nvenc-opts-parser.c
          nvenc-properties.c
          nvenc.c
          obs-nvenc.c
          obs-nvenc.h)

target_link_libraries(obs-nvenc PRIVATE OBS::libobs OBS::opts-parser FFnvcodec::FFnvcodec
                                        $<$<PLATFORM_ID:Linux>:OBS::glad>)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in obs-nvenc.rc)
  target_sources(obs-nvenc PRIVATE obs-nvenc.rc)
endif()

# cmake-format: off
set_target_properties_obs(obs-nvenc PROPERTIES FOLDER plugins/obs-nvenc PREFIX "")
# cmake-format: on
