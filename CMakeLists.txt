cmake_minimum_required(VERSION 3.10)
project(c_tools_lsp C)

option(WITH_UNIT_TESTS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED IMPORTED_TARGET json-c)

find_library(UBOX ubox REQUIRED)

add_subdirectory(src)

if(WITH_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
