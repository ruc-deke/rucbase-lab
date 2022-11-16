set(SOURCES execution_manager.cpp)
add_library(execution STATIC ${SOURCES})

target_link_libraries(execution system record system transaction)



## exec_sql
add_executable(exec_sql exec_sql.cpp)
target_link_libraries(exec_sql execution parser gtest_main)
