cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(previoustube)

idf_build_set_property(COMPILE_OPTIONS "-I../main" APPEND)
