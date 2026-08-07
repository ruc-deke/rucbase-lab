add_library(rucbase_wire STATIC wire.cpp client.cpp)
add_library(rucbase::wire ALIAS rucbase_wire)
target_include_directories(rucbase_wire PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(rucbase_wire
        PUBLIC Threads::Threads
        PRIVATE rucbase::project_options)
