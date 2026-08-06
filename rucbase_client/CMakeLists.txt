# Staff-provided SQL client.
# Built from the main repository presets:
#   cmake --preset debug
#   cmake --build --preset debug --target rucbase_client
# Or standalone (requires sibling src/net):
#   cmake -S rucbase_client -B rucbase_client/build && cmake --build rucbase_client/build

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rucbase_client LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    find_package(Threads REQUIRED)

    add_library(rucbase_wire STATIC ../src/net/wire.cpp)
    target_include_directories(rucbase_wire PUBLIC ../src)
    target_compile_features(rucbase_wire PUBLIC cxx_std_17)
endif()

find_library(READLINE_LIBRARY readline REQUIRED)
find_path(READLINE_INCLUDE_DIR readline/readline.h REQUIRED)

add_executable(rucbase_client main.cpp)
target_include_directories(rucbase_client PRIVATE ${READLINE_INCLUDE_DIR})
target_compile_features(rucbase_client PRIVATE cxx_std_17)
target_compile_options(rucbase_client PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall -Wextra>)
target_link_libraries(rucbase_client PRIVATE Threads::Threads ${READLINE_LIBRARY} rucbase_wire)

install(TARGETS rucbase_client RUNTIME DESTINATION bin)
