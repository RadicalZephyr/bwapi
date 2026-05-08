cmake_minimum_required(VERSION 3.14)

#
# The BWAPILIB Coverage target is in here because gcov/lcov cannot include sources outside of its directory scope
#
# Usage: cmake --build . --config Debug --target BWAPILIB_coverage
if(CMAKE_COMPILER_IS_GNUCXX AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL ""))
  include(CodeCoverage)

  setup_target_for_coverage_lcov(
    NAME BWAPILIB_coverage
    EXECUTABLE BWAPILIBUnitTest --gtest_shuffle
    DEPENDENCIES BWAPILIB
    BASE_DIRECTORY ${BWAPILIB_SOURCE_DIR}
    EXCLUDE "${CMAKE_BINARY_DIR}/*" "../*Test/*" "../*Tests/*" "/usr/*"
    LCOV_ARGS --rc branch_coverage=1 --ignore-errors mismatch,unused
    GENHTML_ARGS --rc genhtml_branch_coverage=1
  )

  setup_target_for_coverage_lcov(
    NAME FunctionalTests_coverage
    EXECUTABLE FunctionalTests --gtest_shuffle
    DEPENDENCIES BWAPILIB
    BASE_DIRECTORY ${BWAPILIB_SOURCE_DIR}
    EXCLUDE "${CMAKE_BINARY_DIR}/*" "../*Test/*" "../*Tests/*" "/usr/*"
    LCOV_ARGS --rc branch_coverage=1 --ignore-errors mismatch,unused
    GENHTML_ARGS --rc genhtml_branch_coverage=1
  )

endif()
