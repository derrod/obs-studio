cmake_minimum_required(VERSION 3.16...3.25)

add_library(QtLottie INTERFACE)
add_library(OBS::QtLottie ALIAS QtLottie)

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)
find_package(rlottie)

if(NOT rlottie_FOUND)
  message("rlottie not found, disabled lottie support.")
  return()
endif()

target_link_libraries(QtLottie INTERFACE Qt6::Gui Qt6::Widgets rlottie::rlottie)

target_include_directories(QtLottie INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_sources(QtLottie INTERFACE src/qtlottiedrawengine.h src/qtlottiewidget.h src/qtlottiedrawengine.cpp
                                  src/qtlottiewidget.cpp)

set_target_properties(QtLottie PROPERTIES FOLDER deps)
