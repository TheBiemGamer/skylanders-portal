# skylanders-portal: Skylanders Portal of Power emulation for ReXGlue recompilations.
#
# skylanders_portal_core has no ReXGlue dependency and is unit-tested. It still needs the AES and
# MD5 sources the ReXGlue SDK vendors (for figure_stats), so REXSDK_DIR must point at an SDK tree.
# skylanders_portal_rex (added below when available) needs the rex::runtime target.

cmake_minimum_required(VERSION 3.25)
project(skylanders_portal LANGUAGES CXX C)

if(NOT REXSDK_DIR)
    message(FATAL_ERROR "skylanders-portal: set REXSDK_DIR to a rexglue-sdk source tree")
endif()

add_library(skylanders_portal_core STATIC
    src/portal/audio.cpp
    src/portal/figure_catalog.cpp
    src/portal/figure_file.cpp
    src/portal/figure_stats.cpp
    src/portal/portal_mode.cpp
    src/portal/xbox_frame.cpp
    src/portal/software/software_portal.cpp
    "${REXSDK_DIR}/thirdparty/aes_128/aes.c"
    "${REXSDK_DIR}/thirdparty/libmspack/cabextract/md5.c"
)
target_include_directories(skylanders_portal_core PUBLIC src)
target_include_directories(skylanders_portal_core PRIVATE
    "${REXSDK_DIR}/thirdparty/aes_128"
    "${REXSDK_DIR}/thirdparty/libmspack/cabextract")
target_compile_features(skylanders_portal_core PUBLIC cxx_std_23)

option(SKYLANDERS_PORTAL_TESTS "Build skylanders-portal unit tests" ON)
if(SKYLANDERS_PORTAL_TESTS)
    enable_testing()
    foreach(t portal_framing_test portal_mode_test software_portal_test figure_file_test
              figure_catalog_test figure_stats_test audio_test)
        add_executable(skylanders_${t} tests/${t}.cpp)
        target_include_directories(skylanders_${t} PRIVATE tests)
        target_link_libraries(skylanders_${t} PRIVATE skylanders_portal_core)
        add_test(NAME skylanders_${t} COMMAND skylanders_${t})
    endforeach()
endif()
