include_guard(GLOBAL)

set(RUCBASE_LLVM_VERSION "18" CACHE STRING
        "Preferred LLVM major version for clang-format and clang-tidy")
set(RUCBASE_QUALITY_JOBS "2" CACHE STRING
        "Maximum number of parallel clang-tidy processes")
set(RUCBASE_CLANG_TIDY_WARNINGS_AS_ERRORS
        "clang-analyzer-*,bugprone-*,performance-*,portability-*"
        CACHE STRING "clang-tidy checks promoted to errors by quality targets")

set(RUCBASE_LLVM_SEARCH_PATHS
        "/opt/homebrew/opt/llvm@${RUCBASE_LLVM_VERSION}/bin"
        "/opt/homebrew/opt/llvm/bin"
        "/usr/local/opt/llvm@${RUCBASE_LLVM_VERSION}/bin"
        "/usr/local/opt/llvm/bin")

find_program(RUCBASE_CLANG_FORMAT_EXECUTABLE
        NAMES "clang-format-${RUCBASE_LLVM_VERSION}" clang-format
        HINTS ${RUCBASE_LLVM_SEARCH_PATHS})
find_program(RUCBASE_CLANG_TIDY_EXECUTABLE
        NAMES "clang-tidy-${RUCBASE_LLVM_VERSION}" clang-tidy
        HINTS ${RUCBASE_LLVM_SEARCH_PATHS})
find_program(RUCBASE_RUN_CLANG_TIDY_EXECUTABLE
        NAMES "run-clang-tidy-${RUCBASE_LLVM_VERSION}" run-clang-tidy
        HINTS ${RUCBASE_LLVM_SEARCH_PATHS})

function(rucbase_add_missing_tool_target target_name tool_name)
    add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -E echo
            "${tool_name} is required for '${target_name}'. See docs/RUCBase开发文档.md."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM)
endfunction()

if(RUCBASE_CLANG_FORMAT_EXECUTABLE)
    execute_process(
            COMMAND "${RUCBASE_CLANG_FORMAT_EXECUTABLE}" --version
            OUTPUT_VARIABLE RUCBASE_CLANG_FORMAT_VERSION_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if(NOT RUCBASE_CLANG_FORMAT_VERSION_OUTPUT MATCHES
            "version[ ]+${RUCBASE_LLVM_VERSION}\\.")
        message(STATUS
                "Quality checks prefer LLVM ${RUCBASE_LLVM_VERSION}, but found: "
                "${RUCBASE_CLANG_FORMAT_VERSION_OUTPUT}")
    endif()
endif()

file(GLOB_RECURSE RUCBASE_CXX_QUALITY_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/rucbase_client/*.cpp"
        "${PROJECT_SOURCE_DIR}/rucbase_client/*.h")
list(FILTER RUCBASE_CXX_QUALITY_FILES EXCLUDE REGEX
        "/(build|cmake-build[^/]*)/")

if(RUCBASE_CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format-cpp
            COMMAND "${RUCBASE_CLANG_FORMAT_EXECUTABLE}" -i
            ${RUCBASE_CXX_QUALITY_FILES}
            COMMENT "Formatting all RUCBase C/C++ sources"
            VERBATIM)
    add_custom_target(check-format-cpp
            COMMAND "${RUCBASE_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror
            ${RUCBASE_CXX_QUALITY_FILES}
            COMMENT "Checking all RUCBase C/C++ sources"
            VERBATIM)
else()
    rucbase_add_missing_tool_target(format-cpp clang-format)
    rucbase_add_missing_tool_target(check-format-cpp clang-format)
endif()

set(RUCBASE_TIDY_SOURCE_PATTERN "^${PROJECT_SOURCE_DIR}/(src|rucbase_client)/.*\\.cpp$")
set(RUCBASE_TIDY_PLATFORM_ARGUMENTS)
if(APPLE)
    # Homebrew clang-tidy does not automatically inherit AppleClang's SDK include paths.
    foreach(include_directory IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
        list(APPEND RUCBASE_TIDY_PLATFORM_ARGUMENTS
                "-extra-arg-before=-isystem${include_directory}")
    endforeach()
endif()

if(RUCBASE_CLANG_TIDY_EXECUTABLE AND RUCBASE_RUN_CLANG_TIDY_EXECUTABLE)
    add_custom_target(check-clang-tidy
            COMMAND "${RUCBASE_RUN_CLANG_TIDY_EXECUTABLE}"
            -clang-tidy-binary "${RUCBASE_CLANG_TIDY_EXECUTABLE}"
            -p "${CMAKE_BINARY_DIR}"
            -warnings-as-errors "${RUCBASE_CLANG_TIDY_WARNINGS_AS_ERRORS}"
            -j "${RUCBASE_QUALITY_JOBS}"
            -quiet
            ${RUCBASE_TIDY_PLATFORM_ARGUMENTS}
            "${RUCBASE_TIDY_SOURCE_PATTERN}"
            COMMENT "Running clang-tidy on all RUCBase translation units"
            VERBATIM)
else()
    rucbase_add_missing_tool_target(check-clang-tidy "clang-tidy and run-clang-tidy")
endif()

add_custom_target(format DEPENDS format-cpp)
add_custom_target(check-format DEPENDS check-format-cpp)
add_custom_target(check-quality DEPENDS
        check-clang-tidy
        check-format-cpp)
