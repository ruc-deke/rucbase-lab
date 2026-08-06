set(SOURCES sm_manager.cpp)
add_library(system STATIC ${SOURCES})
target_link_libraries(system PRIVATE index record)
rucbase_configure_target(system)
