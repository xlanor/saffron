cmake_minimum_required(VERSION 3.13)

set(PLATFORM_SWITCH ON CACHE BOOL "Build for Nintendo Switch" FORCE)

set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

set(USE_DEKO3D ON CACHE BOOL "Use deko3d for video rendering" FORCE)

include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(saffron)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_REVISION "1")
set(APP_TITLE "Saffron")
set(PROJECT_AUTHOR "xlanor")
set(PACKAGE_NAME "io.github.saffron")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}")

list(APPEND APP_PLATFORM_OPTION
    -DBUILD_PACKAGE_NAME=${PACKAGE_NAME}
)

file(WRITE ${PROJECT_RESOURCES}/version.txt "${APP_VERSION}")

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

set(BRLS_RESOURCES_DIR "romfs:/")

if (PLATFORM_SWITCH)
    find_package(PkgConfig REQUIRED)

    pkg_search_module(CURL REQUIRED libcurl)
    find_library(CURL_LIBRARY curl PATHS ${CURL_LIBRARY_DIRS} NO_DEFAULT_PATH)
    if(NOT CURL_LIBRARY)
        list(GET CURL_LIBRARY_DIRS 0 CURL_FIRST_LIB_DIR)
        set(CURL_LIBRARY "${CURL_FIRST_LIB_DIR}/libcurl.a")
    endif()

    find_library(ZSTD_LIB zstd)

    add_library(CURL::libcurl STATIC IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION "${CURL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${ZSTD_LIB}"
    )

    list(APPEND APP_PLATFORM_INCLUDE ${CURL_INCLUDE_DIRS})

    pkg_search_module(AVCODEC REQUIRED libavcodec)
    pkg_search_module(AVFORMAT REQUIRED libavformat)
    pkg_search_module(AVUTIL REQUIRED libavutil)
    pkg_search_module(SWSCALE REQUIRED libswscale)
    list(APPEND APP_PLATFORM_INCLUDE
        ${AVCODEC_INCLUDE_DIRS}
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
    )

    find_library(SDL2_LIB SDL2)
    if(NOT SDL2_LIB)
        message(FATAL_ERROR "SDL2 not found")
    endif()

    find_library(SWRESAMPLE_LIB swresample)

    pkg_search_module(MPV REQUIRED mpv)
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_INCLUDE_DIRS})
    list(APPEND APP_PLATFORM_LIB ${MPV_LIBRARIES})
    link_directories(${MPV_LIBRARY_DIRS})
endif()

set(MAIN_SRC
    source/main.cpp
    source/core/settings_manager.cpp
    source/core/plex_server.cpp
    source/core/plex_api.cpp
    source/core/auth_manager.cpp
    source/core/server_discovery.cpp
    source/core/mpv_core.cpp
    source/models/plex_types.cpp
    source/util/shared_view_holder.cpp
    source/util/overclock.cpp
    source/views/settings_tab.cpp
    source/views/server_list_tab.cpp
    source/views/config_view_tab.cpp
    source/views/home_tab.cpp
    source/views/search_tab.cpp
    source/views/search_view.cpp
    source/views/library_tab.cpp
    source/views/library_browse_view.cpp
    source/views/library_section_tab.cpp
    source/views/collections_tab.cpp
    source/views/collection_detail_view.cpp
    source/views/playlists_tab.cpp
    source/views/media_detail_view.cpp
    source/views/tag_media_view.cpp
    source/views/show_detail_view.cpp
    source/views/season_view.cpp
    source/views/media_grid_view.cpp
    source/views/player_view.cpp
    source/views/video_profile.cpp
    source/view/recycling_grid.cpp
    source/view/h_recycling_grid.cpp
    source/view/cast_card_cell.cpp
    source/view/focusable_card.cpp
    source/view/media_card_data_source.cpp
    source/views/player_activity.cpp
    source/views/video_view.cpp
)

list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)

add_subdirectory(${BOREALIS_DIR}/library borealis)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../references/akira/library/tomlplusplus tomlplusplus EXCLUDE_FROM_ALL)

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
    ${APP_PLATFORM_INCLUDE}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -ffunction-sections
    -fdata-sections
    -Wunused-variable
    ${APP_PLATFORM_OPTION}
)

if (USE_DEKO3D)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BOREALIS_USE_DEKO3D)
endif()

# Enable DEBUG for nxlink logging (calls nxlinkStdio() in switch_wrapper.c)
target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)

# TEST - add CURL
target_link_libraries(${PROJECT_NAME} PRIVATE
    borealis
    tomlplusplus::tomlplusplus
    CURL::libcurl
    ${APP_PLATFORM_LIB}
)

if (PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE EGL glapi drm_nouveau)
endif()

target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})

# Compile deko3d shaders for NanoVG (required for borealis rendering)
if (USE_DEKO3D)
    set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/resources/shaders")
    gen_dksh("${SHADER_OUT_DIR}")
    message(STATUS "Compiled NanoVG deko3d shaders to ${SHADER_OUT_DIR}")
endif()

set(BUILD_FONT_DIR ${CMAKE_BINARY_DIR}/resources/font)
add_custom_target(${PROJECT_NAME}.nro
    DEPENDS ${PROJECT_NAME}
    COMMAND ${NX_NACPTOOL_EXE} --create "${APP_TITLE}" "${PROJECT_AUTHOR}" "${APP_VERSION}" ${PROJECT_NAME}.nacp
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_DIR}/resources/material ${CMAKE_BINARY_DIR}/resources/material
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
        --icon=${PROJECT_ICON}
        --nacp=${PROJECT_NAME}.nacp
        --romfsdir=${CMAKE_BINARY_DIR}/resources
    ALL
)
