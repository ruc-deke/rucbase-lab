include(GoogleTest)

function(rucbase_discover_gtests target_name)
    set(test_working_directory "${CMAKE_CURRENT_BINARY_DIR}/${target_name}")
    file(MAKE_DIRECTORY "${test_working_directory}")
    gtest_discover_tests(${target_name}
            TEST_PREFIX "${target_name}."
            WORKING_DIRECTORY "${test_working_directory}"
            PROPERTIES LABELS unit RUN_SERIAL TRUE)
endfunction()

# storage test
add_executable(disk_manager_test storage/disk_manager_test.cpp)
target_link_libraries(disk_manager_test PRIVATE storage GTest::gtest_main)
rucbase_configure_target(disk_manager_test)
rucbase_discover_gtests(disk_manager_test)

add_executable(lru_replacer_test storage/lru_replacer_test.cpp)
target_link_libraries(lru_replacer_test PRIVATE lru_replacer GTest::gtest_main)
rucbase_configure_target(lru_replacer_test)
rucbase_discover_gtests(lru_replacer_test)

add_executable(buffer_pool_manager_test storage/buffer_pool_manager_test.cpp)
target_link_libraries(buffer_pool_manager_test PRIVATE storage GTest::gtest_main)
rucbase_configure_target(buffer_pool_manager_test)
rucbase_discover_gtests(buffer_pool_manager_test)

add_executable(record_manager_test storage/record_manager_test.cpp)
target_link_libraries(record_manager_test PRIVATE record GTest::gtest_main)
rucbase_configure_target(record_manager_test)
rucbase_discover_gtests(record_manager_test)

# index test
add_executable(b_plus_tree_insert_test index/b_plus_tree_insert_test.cpp)
target_link_libraries(b_plus_tree_insert_test PRIVATE system index GTest::gtest_main)
rucbase_configure_target(b_plus_tree_insert_test)
rucbase_discover_gtests(b_plus_tree_insert_test)

add_executable(b_plus_tree_delete_test index/b_plus_tree_delete_test.cpp)
target_link_libraries(b_plus_tree_delete_test PRIVATE system index GTest::gtest_main)
rucbase_configure_target(b_plus_tree_delete_test)
rucbase_discover_gtests(b_plus_tree_delete_test)

add_executable(b_plus_tree_concurrent_test index/b_plus_tree_concurrent_test.cpp)
target_link_libraries(b_plus_tree_concurrent_test PRIVATE system index GTest::gtest_main)
rucbase_configure_target(b_plus_tree_concurrent_test)
rucbase_discover_gtests(b_plus_tree_concurrent_test)

# wire protocol tests (independent of the storage/query lab implementations)
add_executable(wire_test net/wire_test.cpp)
target_link_libraries(wire_test PRIVATE rucbase_wire GTest::gtest_main Threads::Threads)
rucbase_configure_target(wire_test)
rucbase_discover_gtests(wire_test)

# query test
add_executable(query_test query/query_test.cpp)
target_link_libraries(query_test PRIVATE rucbase_wire)
rucbase_configure_target(query_test)

# transaction test
add_executable(transaction_test transaction/transaction_test.cpp)
target_link_libraries(transaction_test PRIVATE readline rucbase_wire)
rucbase_configure_target(transaction_test)

# regress test
add_executable(regress_test regress/regress_test_main.cpp regress/regress_test.cpp)
target_link_libraries(regress_test PRIVATE rucbase_wire)
rucbase_configure_target(regress_test)

# concurrency test
add_executable(concurrency_test concurrency/concurrency_test_main.cpp concurrency/concurrency_test.cpp regress/regress_test.cpp)
target_link_libraries(concurrency_test PRIVATE rucbase_wire)
rucbase_configure_target(concurrency_test)

# The remaining C++ test programs are black-box clients. Pytest starts an rmdb
# process, gives each test its own database and port, then invokes these clients.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "import pytest"
            RESULT_VARIABLE RUCBASE_PYTEST_IMPORT_RESULT
            OUTPUT_QUIET
            ERROR_QUIET)
endif()

if(Python3_Interpreter_FOUND AND RUCBASE_PYTEST_IMPORT_RESULT EQUAL 0)
    set(RUCBASE_BLACKBOX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/blackbox")
    set(RUCBASE_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/test-logs")

    function(rucbase_add_blackbox_test test_name)
        set(pytest_nodes)
        foreach(node IN LISTS ARGN)
            list(APPEND pytest_nodes "${RUCBASE_BLACKBOX_DIR}/${node}")
        endforeach()
        add_test(NAME "${test_name}"
                COMMAND "${Python3_EXECUTABLE}" -m pytest -q
                ${pytest_nodes}
                --rmdb-binary "$<TARGET_FILE:rmdb>"
                --query-client "$<TARGET_FILE:query_test>"
                --transaction-client "$<TARGET_FILE:transaction_test>"
                --regress-client "$<TARGET_FILE:regress_test>"
                --concurrency-client "$<TARGET_FILE:concurrency_test>"
                --log-dir "${RUCBASE_TEST_LOG_DIR}")
        set_tests_properties("${test_name}" PROPERTIES LABELS blackbox TIMEOUT 180)
    endfunction()

    # Handout-compatible "basic" suites (match transaction_test.py / concurrency_test.py).
    # Bonus cases are registered separately so students who only finished basics pass CTest.
    rucbase_add_blackbox_test(blackbox.query "test_query.py::test_query")
    rucbase_add_blackbox_test(blackbox.regress "test_query.py::test_regress_client")
    rucbase_add_blackbox_test(blackbox.transaction
            "test_transaction.py::test_transaction[commit_test]"
            "test_transaction.py::test_transaction[abort_test]")
    rucbase_add_blackbox_test(blackbox.transaction.bonus
            "test_transaction.py::test_transaction[commit_index_test]"
            "test_transaction.py::test_transaction[abort_index_test]")
    rucbase_add_blackbox_test(blackbox.concurrency
            "test_concurrency.py::test_concurrency[concurrency_read_test]"
            "test_concurrency.py::test_concurrency[dirty_write_test]"
            "test_concurrency.py::test_concurrency[dirty_read_test]"
            "test_concurrency.py::test_concurrency[lost_update_test]"
            "test_concurrency.py::test_concurrency[unrepeatable_read_test]"
            "test_concurrency.py::test_concurrency[unrepeatable_read_test_hard]")
    rucbase_add_blackbox_test(blackbox.concurrency.bonus
            "test_concurrency.py::test_concurrency[phantom_read_test_1]"
            "test_concurrency.py::test_concurrency[phantom_read_test_2]"
            "test_concurrency.py::test_concurrency[phantom_read_test_3]"
            "test_concurrency.py::test_concurrency[phantom_read_test_4]")
else()
    message(STATUS "pytest was not found; Python black-box tests are not registered")
endif()
