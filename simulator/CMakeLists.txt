cmake_minimum_required(VERSION 3.16)
project(simulator)
include(FetchContent)
set(CMAKE_CXX_STANDARD 20)

set(LV_CONF_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
        CACHE STRING "" FORCE)

FetchContent_Declare(
        lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG release/v8.3
        GIT_SHALLOW TRUE
)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.2
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2 lvgl)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)

include_directories(${SDL2_INCLUDE_DIRS} . ${lv_drivers_SOURCE_DIR} ${SDL2_SOURCE_DIRS}/include ../main ../components/fpm/include)

add_executable(previoustube_simulator
        simulator_main.cpp
        ../main/clock.cpp
        ../main/fonts/oswald_60.c
        ../main/fonts/oswald_100.c
        ../main/flapper.cpp
        ../components/fpm/include/fpm/fixed.hpp
        ../components/fpm/include/fpm/math.hpp)

target_link_libraries(previoustube_simulator PRIVATE lvgl::lvgl SDL2::SDL2main SDL2::SDL2-static)
