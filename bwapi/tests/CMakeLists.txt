# The host-buildable half of BWAPI's server side.
#
# BWAPI.dll itself is Windows-only and reaches into StarCraft's address space, so it cannot be
# built here. What can be built here is everything the trusted side decides without touching the
# game: the validation of client-written shared memory, and the handle allocation that decides
# what a bot may name. Those are where the defects in ADR 0001 section 2 live, so this is where
# they get tested.
#
#   cmake -B build -S tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build && ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(bwapi_server_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT MSVC)
  add_compile_options(-Wall -Wextra -Werror)
endif()

# Sanitizers are the point of running these on a host at all: the defects being fixed are
# out-of-bounds reads and writes driven by untrusted input.
option(BWAPI_TEST_SANITIZERS "Build the tests with ASan and UBSan" OFF)
if (BWAPI_TEST_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()

function(bwapi_add_test name)
  add_executable(${name} ${name}.cpp)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../BWAPI/Source/BWAPI)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

bwapi_add_test(client_input_test)
