# Staff-provided SQL client.
# Built from the main repository presets:
#   cmake --preset debug
#   cmake --build --preset debug --target rucbase_client
# Or standalone (requires sibling src/net):
#   cmake -S rucbase_client -B rucbase_client/build && cmake --build rucbase_client/build

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rucbase_client LANGUAGES CXX)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    find_package(Threads REQUIRED)

    add_library(rucbase_wire STATIC ../src/net/wire.cpp ../src/net/client.cpp)
    add_library(rucbase::wire ALIAS rucbase_wire)
    target_include_directories(rucbase_wire PUBLIC ../src)
    target_compile_features(rucbase_wire PUBLIC cxx_std_17)
    target_link_libraries(rucbase_wire PUBLIC Threads::Threads)
endif()

find_library(READLINE_LIBRARY readline REQUIRED)
find_path(READLINE_INCLUDE_DIR readline/readline.h REQUIRED)
if(NOT TARGET Readline::Readline)
    add_library(Readline::Readline UNKNOWN IMPORTED)
    set_target_properties(Readline::Readline PROPERTIES
            IMPORTED_LOCATION "${READLINE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${READLINE_INCLUDE_DIR}")
endif()
mark_as_advanced(READLINE_LIBRARY READLINE_INCLUDE_DIR)

add_executable(rucbase_client main.cpp)
target_link_libraries(rucbase_client PRIVATE rucbase::wire Readline::Readline)
if(TARGET rucbase::project_options)
    target_link_libraries(rucbase_client PRIVATE rucbase::project_options)
else()
    target_compile_features(rucbase_client PRIVATE cxx_std_17)
    target_compile_options(rucbase_client PRIVATE
            $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>)
endif()

install(TARGETS rucbase_client RUNTIME DESTINATION bin)
