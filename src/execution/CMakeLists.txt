set(SOURCES execution_manager.cpp)
add_library(execution STATIC ${SOURCES})

target_link_libraries(execution PRIVATE system record transaction)
rucbase_configure_target(execution)
