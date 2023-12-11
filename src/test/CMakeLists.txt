# storage test
add_executable(disk_manager_test storage/disk_manager_test.cpp)
target_link_libraries(disk_manager_test storage gtest_main)

add_executable(lru_replacer_test storage/lru_replacer_test.cpp)
target_link_libraries(lru_replacer_test lru_replacer gtest_main)

add_executable(buffer_pool_manager_test storage/buffer_pool_manager_test.cpp)
target_link_libraries(buffer_pool_manager_test storage gtest_main)

add_executable(record_manager_test storage/record_manager_test.cpp)
target_link_libraries(record_manager_test record gtest_main)

# index test
add_executable(b_plus_tree_insert_test index/b_plus_tree_insert_test.cpp)
target_link_libraries(b_plus_tree_insert_test system index gtest_main)

add_executable(b_plus_tree_delete_test index/b_plus_tree_delete_test.cpp)
target_link_libraries(b_plus_tree_delete_test system index gtest_main)

add_executable(b_plus_tree_concurrent_test index/b_plus_tree_concurrent_test.cpp)
target_link_libraries(b_plus_tree_concurrent_test system index gtest_main)

# query test
add_executable(query_test query/query_test.cpp)

# transaction test
add_executable(transaction_test transaction/transaction_test.cpp)
target_link_libraries(transaction_test readline)

# regress test
add_executable(regress_test regress/regress_test_main.cpp regress/regress_test.cpp)

# concurrency test
add_executable(concurrency_test concurrency/concurrency_test_main.cpp concurrency/concurrency_test.cpp regress/regress_test.cpp)

