set(SOURCES log_manager.cpp log_recovery.cpp)
add_library(recovery STATIC ${SOURCES})
add_library(recoverys SHARED ${SOURCES})
target_link_libraries(recovery system pthread)
