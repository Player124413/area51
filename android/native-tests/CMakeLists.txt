#===============================================================================
#
#  CMakeLists.txt - host build of the native core unit tests.
#
#  The Android build has its own CMakeLists in app/src/main/cpp; this one only
#  exists so the tests can also be run from an IDE.  The shell script
#  run_tests.sh does the same job without CMake.
#
#===============================================================================

cmake_minimum_required( VERSION 3.16 )
project( a51_native_tests CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp )

set( CORE_SOURCES
    ${CORE_DIR}/a51/crc16.cpp
    ${CORE_DIR}/a51/dfs_archive.cpp
    ${CORE_DIR}/a51/input_gadgets.cpp
    ${CORE_DIR}/a51/input_state.cpp
    ${CORE_DIR}/a51/json.cpp
    ${CORE_DIR}/a51/perf_governor.cpp
    ${CORE_DIR}/a51/touch_layout.cpp
)

enable_testing()

foreach( SUITE test_dfs test_perf test_touch test_json )
    add_executable( ${SUITE} ${SUITE}.cpp ${CORE_SOURCES} )
    target_include_directories( ${SUITE} PRIVATE ${CORE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )
    target_compile_options( ${SUITE} PRIVATE -Wall -Wextra -Wno-unused-parameter )
    add_test( NAME ${SUITE} COMMAND ${SUITE} )
endforeach()
