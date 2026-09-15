# Fork-local. Upstream bwapi/bwapi has no top-level CMake project: CMake/ holds two
# subdirectories that a bot's own build add_subdirectory()s, which is what CMake/README.md
# documents and all it was ever meant to do.
#
# This file exists so that docs/decisions/experiments/ has a build to join -- an ADR that cites a
# number has to leave behind code a reader can run, and code nothing builds is code that rots.
# See docs/decisions/experiments/README.md. It is deliberately thin: it adds no compile flags, no
# install rules and no packaging, and it does not try to become the way BWAPI is built. That is
# still bwapi/bwapi.sln under MSVC.
cmake_minimum_required(VERSION 3.21)

project(BWAPI CXX)

# ------------------------------------------------------------------------------------------------
# Headers
#
# BWAPI's headers are the part of it that works everywhere. The implementation is not: it needs
# <windows.h>, __stdcall and MSVC's flags. An experiment that only reads headers -- a GameData
# field offset, a sizeof table, a struct's packing, what a calling convention does to an argument
# -- compiles against this target and nothing else, including from Linux with clang targeting the
# MSVC ABI. That covers most of what an ADR here actually needs to measure.
add_library(bwapi_headers INTERFACE)
target_include_directories(bwapi_headers INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/bwapi/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/bwapi/Util/Source"
)
add_library(BWAPI::headers ALIAS bwapi_headers)

# ------------------------------------------------------------------------------------------------
# Client-mode static libraries
#
# CMake/BWAPI and CMake/Client build the two static libraries a client-mode bot links. They are
# MSVC-shaped -- CMake/BWAPI passes /DNOMINMAX unconditionally and the sources want <windows.h> --
# so they are off anywhere but Windows rather than left to fail at the first compile. Turning this
# on elsewhere is not forbidden; it is just not expected to work, and finding out is the point of
# turning it on.
if(WIN32)
  set(BWAPI_BUILD_CLIENT_LIBS_DEFAULT ON)
else()
  set(BWAPI_BUILD_CLIENT_LIBS_DEFAULT OFF)
endif()
option(BWAPI_BUILD_CLIENT_LIBS
  "Build the client-mode static libraries under CMake/ (Windows/MSVC)"
  ${BWAPI_BUILD_CLIENT_LIBS_DEFAULT})

if(BWAPI_BUILD_CLIENT_LIBS)
  add_subdirectory(CMake/BWAPI)
  add_subdirectory(CMake/Client)
endif()

# ------------------------------------------------------------------------------------------------
# ADR experiments
#
# Built by default when BWAPI is the top-level project, because the only check this repository can
# give an experiment is that it has to compile (docs/decisions/experiments/README.md). Off when
# BWAPI is a subdirectory of something else, which is how upstream's CMake is normally consumed.
option(BWAPI_BUILD_EXPERIMENTS
  "Build the experiments backing docs/decisions/records/"
  ${PROJECT_IS_TOP_LEVEL})

if(BWAPI_BUILD_EXPERIMENTS)
  add_subdirectory(docs/decisions/experiments)
endif()
