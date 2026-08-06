option(RUCBASE_FETCH_DEPENDENCIES "Download missing third-party dependencies during configuration" ON)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/googletest/CMakeLists.txt")
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/googletest" "${CMAKE_BINARY_DIR}/_deps/googletest-build")
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
