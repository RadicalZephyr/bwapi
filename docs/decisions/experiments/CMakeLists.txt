# The experiments backing docs/decisions/records/. README.md beside this file has the rules,
# including the retirement rule that says this directory should trend toward empty.
#
# Add an experiment with one line:
#
#     bwapi_add_experiment(0001-some-decision)          # headers only; builds anywhere
#     bwapi_add_experiment(0002-other-decision CLIENT)  # + the client static libs; Windows only
#
# naming it after the record it serves, and run it from the repository root with
#
#     cmake --build build --target run-0001-some-decision

# Declare one experiment. NAME is the record's number and slug, and is both the source file stem
# (NAME.cpp) and the target name. CLIENT says the experiment needs the client-mode static
# libraries rather than only BWAPI's headers, which confines it to Windows; without it the
# experiment links bwapi_headers and builds anywhere clang or MSVC does.
function(bwapi_add_experiment NAME)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "CLIENT" "" "")

  if(ARG_CLIENT AND NOT BWAPI_BUILD_CLIENT_LIBS)
    # Skip rather than fail. A Linux checkout should still configure and build every experiment
    # that can build, and the record citing this one says which platform produced its numbers.
    message(STATUS "Experiment ${NAME}: skipped, needs BWAPI_BUILD_CLIENT_LIBS")
    return()
  endif()

  add_executable(${NAME} "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cpp")
  target_link_libraries(${NAME} PRIVATE bwapi_headers)
  if(ARG_CLIENT)
    add_dependencies(${NAME} BWAPI-Static BWAPIClient)
  endif()

  # Experiments are evidence, not tests: they are not registered with CTest and nothing asserts on
  # their output. They print, a record quotes what they printed, and this target is how a reader
  # re-derives the quote.
  add_custom_target(run-${NAME}
    COMMAND ${NAME}
    DEPENDS ${NAME}
    COMMENT "Running experiment ${NAME}"
    USES_TERMINAL)
endfunction()

# ------------------------------------------------------------------------------------------------
# Experiments live below this line, one call each, and are deleted or promoted when the record
# they serve logs Implemented or Deprecated. Empty is the healthy state.
