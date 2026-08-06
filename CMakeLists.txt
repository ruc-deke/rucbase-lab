cmake_minimum_required(VERSION 3.22)
project(RMDB LANGUAGES C CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "Rucbase requires GCC 11 or newer (GCC 13 is recommended).")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "Rucbase requires Clang 14 or newer.")
endif()

option(RUCBASE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(RUCBASE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(RUCBASE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(RUCBASE_ENABLE_TSAN AND (RUCBASE_ENABLE_ASAN OR RUCBASE_ENABLE_UBSAN))
    message(FATAL_ERROR "ThreadSanitizer must use a separate build from ASan/UBSan.")
endif()

add_library(rucbase_build_options INTERFACE)
target_compile_features(rucbase_build_options INTERFACE cxx_std_17)
target_compile_options(rucbase_build_options INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
        # Lab skeletons intentionally leave members unused until students implement them.
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-unused-private-field>)

# Circular static deps (system ↔ record ↔ transaction) cause Apple ld to re-list
# the same archives; the warning is noise for this teaching graph.
if(APPLE)
    target_link_options(rucbase_build_options INTERFACE
            "LINKER:-no_warn_duplicate_libraries")
endif()

if(RUCBASE_ENABLE_ASAN OR RUCBASE_ENABLE_UBSAN OR RUCBASE_ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(FATAL_ERROR "The sanitizer presets currently require GCC or Clang.")
    endif()

    set(RUCBASE_SANITIZERS "")
    if(RUCBASE_ENABLE_ASAN)
        list(APPEND RUCBASE_SANITIZERS "address")
    endif()
    if(RUCBASE_ENABLE_UBSAN)
        list(APPEND RUCBASE_SANITIZERS "undefined")
    endif()
    if(RUCBASE_ENABLE_TSAN)
        list(APPEND RUCBASE_SANITIZERS "thread")
    endif()
    list(JOIN RUCBASE_SANITIZERS "," RUCBASE_SANITIZER_FLAGS)

    target_compile_options(rucbase_build_options INTERFACE
            -fno-omit-frame-pointer "-fsanitize=${RUCBASE_SANITIZER_FLAGS}")
    target_link_options(rucbase_build_options INTERFACE
            -fno-omit-frame-pointer "-fsanitize=${RUCBASE_SANITIZER_FLAGS}")
endif()

function(rucbase_configure_target target_name)
    target_link_libraries(${target_name} PRIVATE rucbase_build_options)
endfunction()

find_package(Threads REQUIRED)
include(CTest)

if(BUILD_TESTING)
    include(deps/CMakeLists.txt)
endif()
add_subdirectory(src)
# Staff-provided interactive client; also buildable via --target rucbase_client.
add_subdirectory(rucbase_client)
