#===============================================================================
#
#  CMakeLists.txt - Android build of liba51.so
#
#  Layout:
#      a51/       core, no Android dependencies (unit tested on a desktop)
#      renderer/  GLES 3
#      .          JNI glue
#
#===============================================================================

cmake_minimum_required( VERSION 3.22.1 )
project( "a51" CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CORE_SOURCES
    a51/crc16.cpp
    a51/dfs_archive.cpp
    a51/input_gadgets.cpp
    a51/input_state.cpp
    a51/json.cpp
    a51/perf_governor.cpp
    a51/touch_layout.cpp
    a51/runtime.cpp
)

set( ANDROID_SOURCES
    renderer/gl_runtime.cpp
    jni_bridge.cpp
)

add_library( a51 SHARED
    ${CORE_SOURCES}
    ${ANDROID_SOURCES}
)

target_include_directories( a51 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )

# -O2 for the release build, -O0 -g for debug.  No exceptions and no RTI: the
# core never uses them and dropping them keeps the .so small.
target_compile_options( a51 PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-exceptions
    -fno-rtti
    -ffunction-sections
    -fdata-sections
    $<$<CONFIG:Release>:-O2 -fvisibility=hidden -flto>
    $<$<CONFIG:Debug>:-O0 -g>
)

target_link_options( a51 PRIVATE
    -Wl,--gc-sections
    $<$<CONFIG:Release>:-flto>
)

find_library( LOG_LIB log )
find_library( GLES3_LIB GLESv3 )

target_link_libraries( a51
    ${LOG_LIB}
    ${GLES3_LIB}
)

# Strip debug symbols from the shipped library.
if( CMAKE_BUILD_TYPE STREQUAL "Release" )
    add_custom_command( TARGET a51 POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:a51>
        COMMENT "Stripping liba51.so" )
endif()
