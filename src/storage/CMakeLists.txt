# storage module
set(SOURCES 
        disk_manager.cpp 
        buffer_pool_manager.cpp 
        ../replacer/replacer.h 
        ../replacer/lru_replacer.cpp 
        ../replacer/clock_replacer.cpp
)
add_library(storage STATIC ${SOURCES})

# disk_manager_test
add_library(disk STATIC disk_manager.cpp)
add_executable(disk_manager_test disk_manager_test.cpp)
target_link_libraries(disk_manager_test disk gtest_main)  # add gtest

# buffer_pool_manager_test
add_executable(buffer_pool_manager_test buffer_pool_manager_test.cpp)
target_link_libraries(buffer_pool_manager_test storage gtest_main)  # add gtest
