cmake_minimum_required(VERSION 3.10)
project(c_tools_lsp C)

option(WITH_UNIT_TESTS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so")

include(FetchContent)

set(LOCAL_LIBRPC2_PATH "" CACHE PATH "Path to local librpc2 repository")

if(DEFINED LOCAL_LIBRPC2_PATH AND EXISTS ${LOCAL_LIBRPC2_PATH})
  if(NOT IS_DIRECTORY ${LOCAL_LIBRPC2_PATH})
    message(FATAL_ERROR "LOCAL_LIBRPC2_PATH is not a valid path: ${LOCAL_LIBRPC2_PATH}")
  endif()

  message(STATUS "Using local librpc2 repository at: ${LOCAL_LIBRPC2_PATH}")
  add_subdirectory(${LOCAL_LIBRPC2_PATH} librpc2-build)
else()
  set(LIBRPC2_GIT_REPO "git@github.com:ChrisNisbet01/librpc2.git")
  set(LIBRPC2_REPO_TAG "master")
  message(STATUS "Using github librpc2 repository at: ${LIBRPC2_GIT_REPO}:${LIBRPC2_REPO_TAG}")
  FetchContent_Declare(
    librpc2
    GIT_REPOSITORY ${LIBRPC2_GIT_REPO}
    GIT_TAG ${LIBRPC2_REPO_TAG}
  )
  FetchContent_MakeAvailable(librpc2)
endif()

add_subdirectory(src)

if(WITH_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
