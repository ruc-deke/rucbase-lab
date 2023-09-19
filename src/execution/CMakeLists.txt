set(SOURCES execution_manager.cpp)
add_library(execution STATIC ${SOURCES})

target_link_libraries(execution system record system transaction)