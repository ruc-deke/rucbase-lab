cmake_minimum_required(VERSION 3.22)
project(RMDB LANGUAGES CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_library(rucbase_project_options INTERFACE)
add_library(rucbase::project_options ALIAS rucbase_project_options)
target_compile_features(rucbase_project_options INTERFACE cxx_std_20)
target_compile_options(rucbase_project_options INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
        # Lab skeletons intentionally leave members unused until students implement them.
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-unused-private-field>)

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

    target_compile_options(rucbase_project_options INTERFACE
            -fno-omit-frame-pointer "-fsanitize=${RUCBASE_SANITIZER_FLAGS}")
    target_link_options(rucbase_project_options INTERFACE
            -fno-omit-frame-pointer "-fsanitize=${RUCBASE_SANITIZER_FLAGS}")
endif()

find_package(Threads REQUIRED)
include(CTest)

# GoogleTest: prefer vendored submodule, then system package, then optional FetchContent.
if(BUILD_TESTING)
    option(RUCBASE_FETCH_DEPENDENCIES
            "Download missing third-party dependencies during configuration" ON)

    if(EXISTS "${PROJECT_SOURCE_DIR}/deps/googletest/CMakeLists.txt")
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        add_subdirectory("${PROJECT_SOURCE_DIR}/deps/googletest"
                "${CMAKE_BINARY_DIR}/_deps/googletest-build" EXCLUDE_FROM_ALL)
    else()
        find_package(GTest CONFIG QUIET)

        if(NOT GTest_FOUND AND RUCBASE_FETCH_DEPENDENCIES)
            include(FetchContent)
            FetchContent_Declare(
                    googletest
                    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
                    TIMEOUT 60)
            set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(googletest)
        endif()
    endif()

    if(NOT TARGET GTest::gtest_main)
        message(FATAL_ERROR
                "GoogleTest was not found. Initialize deps/googletest, install GTest, "
                "or configure with RUCBASE_FETCH_DEPENDENCIES=ON.")
    endif()
endif()

add_subdirectory(src)

# Staff-provided interactive client; also buildable via --target rucbase_client.
add_subdirectory(rucbase_client)
