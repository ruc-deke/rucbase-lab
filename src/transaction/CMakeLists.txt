# txn module
set(SOURCES concurrency/lock_manager.cpp transaction_manager.cpp)
add_library(transaction STATIC ${SOURCES})
target_link_libraries(transaction system recovery pthread)

add_executable(txn_test txn_gtest.cpp)
target_link_libraries(txn_test transaction execution parser gtest_main)

add_executable(lock_test lock_gtest.cpp)
target_link_libraries(lock_test transaction execution gtest_main)

# concurrency_test
add_executable(concurrency_test concurrency_gtest.cpp)
target_link_libraries(concurrency_test transaction execution parser gtest_main)