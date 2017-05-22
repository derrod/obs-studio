project(win-capture)

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

set(COMPAT_UPDATE_URL
	"https://rodney.io/stuff/compat"
	CACHE STRING "Default compatability package URL")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/lookup-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/win-capture/lookup-config.h")

set(win-capture_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/win-capture/lookup-config.h")

set(win-capture_HEADERS
	nt-stuff.h
	obfuscate.h
	app-helpers.h
	hook-helpers.h
	json-helpers.h
	inject-library.h
	cursor-capture.h
	graphics-hook-info.h
	window-helpers.h
	dc-capture.h)

set(win-capture_SOURCES
	dc-capture.c
	obfuscate.c
	app-helpers.c
	inject-library.c
	cursor-capture.c
	game-capture.c
	json-helpers.c
	window-helpers.c
	monitor-capture.c
	window-capture.c
	load-graphics-offsets.c
	duplicator-monitor-capture.c
	plugin-main.c)

add_library(win-capture MODULE
	${win-capture_SOURCES}
	${win-capture_HEADERS})
target_link_libraries(win-capture
	libobs
	ipc-util
	psapi
	file-updater
	${OBS_JANSSON_IMPORT})

target_include_directories(win-capture
	PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/plugins/win-capture>")

install_obs_plugin_with_data(win-capture data)

add_subdirectory(graphics-hook)
add_subdirectory(get-graphics-offsets)
add_subdirectory(inject-helper)
