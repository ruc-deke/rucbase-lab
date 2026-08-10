function(rucbase_add_gtest target_name source dependency)
    add_executable(${target_name} ${source})
    target_link_libraries(${target_name} PRIVATE
            ${dependency}
            GTest::gtest_main
            rucbase::project_options)

    set(test_working_directory "${CMAKE_CURRENT_BINARY_DIR}/${target_name}")
    file(MAKE_DIRECTORY "${test_working_directory}")
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES
            LABELS unit
            WORKING_DIRECTORY "${test_working_directory}")
endfunction()

rucbase_add_gtest(disk_manager_test storage/disk_manager_test.cpp rucbase::core)
rucbase_add_gtest(lru_replacer_test storage/lru_replacer_test.cpp rucbase::core)
rucbase_add_gtest(buffer_pool_manager_test storage/buffer_pool_manager_test.cpp rucbase::core)
rucbase_add_gtest(record_manager_test storage/record_manager_test.cpp rucbase::core)
rucbase_add_gtest(b_plus_tree_insert_test index/b_plus_tree_insert_test.cpp rucbase::core)
rucbase_add_gtest(b_plus_tree_delete_test index/b_plus_tree_delete_test.cpp rucbase::core)
rucbase_add_gtest(b_plus_tree_concurrent_test index/b_plus_tree_concurrent_test.cpp rucbase::core)
rucbase_add_gtest(ix_layout_test index/ix_layout_test.cpp rucbase::core)
rucbase_add_gtest(ix_index_handle_test index/ix_index_handle_test.cpp rucbase::core)
rucbase_add_gtest(rm_record_test record/rm_record_test.cpp rucbase::core)
rucbase_add_gtest(sm_meta_test system/sm_meta_test.cpp rucbase::core)
rucbase_add_gtest(wire_test net/wire_test.cpp rucbase::wire)
rucbase_add_gtest(test_parser parser/parser_test.cpp rucbase::parser)
rucbase_add_gtest(analyze_planner_test query/analyze_planner_test.cpp rucbase::core)
target_link_libraries(analyze_planner_test PRIVATE rucbase::parser)

function(rucbase_add_blackbox_client target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE
            rucbase::wire
            rucbase::project_options)
endfunction()

rucbase_add_blackbox_client(query_test query/query_test.cpp)
rucbase_add_blackbox_client(transaction_test transaction/transaction_test.cpp)
rucbase_add_blackbox_client(regress_test regress/regress_test_main.cpp regress/regress_test.cpp)
rucbase_add_blackbox_client(concurrency_test
        concurrency/concurrency_test_main.cpp
        concurrency/concurrency_test.cpp
        regress/regress_test.cpp)

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
