set(SOURCES ix_node_handle.cpp ix_index_handle.cpp ix_scan.cpp ../common/rwlatch.cpp)
add_library(index STATIC ${SOURCES})
target_link_libraries(index storage)

# insert test
add_executable(b_plus_tree_insert_test b_plus_tree_insert_test.cpp)
target_link_libraries(b_plus_tree_insert_test index gtest_main)

# delete test
add_executable(b_plus_tree_delete_test b_plus_tree_delete_test.cpp)
target_link_libraries(b_plus_tree_delete_test index gtest_main)

# concurrent insert and delete test
add_executable(b_plus_tree_concurrent_test b_plus_tree_concurrent_test.cpp)
target_link_libraries(b_plus_tree_concurrent_test index gtest_main)