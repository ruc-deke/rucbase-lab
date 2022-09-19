# storage module
set(SOURCES 
        disk_manager.cpp 
        buffer_pool_manager.cpp 
        ../replacer/replacer.h 
        ../replacer/lru_replacer.cpp 
        ../replacer/clock_replacer.cpp
)
add_library(storage STATIC ${SOURCES})

 # storage_gtest
 add_executable(storage_gtest storage_gtest.cpp)
 target_link_libraries(storage_gtest storage gtest_main stdc++fs)  # add gtest; stdc++fs for "experimental/filesystem"

 # buffer_pool_manager_test
 add_executable(buffer_pool_manager_test buffer_pool_manager_test.cpp)
 target_link_libraries(buffer_pool_manager_test storage gtest_main)  # add gtest

 # buffer_pool_manager_concurrency_test
 add_executable(buffer_pool_manager_concurrency_test buffer_pool_manager_concurrency_test.cpp)
 target_link_libraries(buffer_pool_manager_concurrency_test storage gtest_main)  # add gtest

 # leaderboard_test
 add_executable(leaderboard_test leaderboard_test.cpp)
 target_link_libraries(leaderboard_test storage gtest_main)  # add gtest
