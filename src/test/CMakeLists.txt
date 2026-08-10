set(RUCBASE_TEST_LABELS
        blackbox
        bonus
        concurrency-suite
        framework
        lab1
        lab2
        lab3
        lab4
        query-suite
        regress-suite
        smoke
        transaction-suite
        unit)

function(rucbase_validate_test_labels test_name)
    foreach(label IN LISTS ARGN)
        if(NOT "${label}" IN_LIST RUCBASE_TEST_LABELS)
            message(FATAL_ERROR "${test_name} uses unknown CTest label: ${label}")
        endif()
    endforeach()
endfunction()

function(rucbase_add_gtest target_name source dependency)
    if(NOT ARGN)
        message(FATAL_ERROR "${target_name} must declare at least one CTest label")
    endif()
    rucbase_validate_test_labels("${target_name}" ${ARGN})

    add_executable(${target_name} ${source})
    target_link_libraries(${target_name} PRIVATE
            ${dependency}
            GTest::gtest_main
            rucbase::project_options)
    # A few white-box teaching tests temporarily expose implementation details.
    set_property(SOURCE ${source} APPEND PROPERTY COMPILE_OPTIONS
            $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-keyword-macro>)

    set(test_working_directory "${CMAKE_CURRENT_BINARY_DIR}/${target_name}")
    file(MAKE_DIRECTORY "${test_working_directory}")
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES
            LABELS "${ARGN}"
            WORKING_DIRECTORY "${test_working_directory}")
endfunction()

rucbase_add_gtest(lab1_disk_manager_test storage/disk_manager_test.cpp rucbase::core unit lab1)
rucbase_add_gtest(lab1_lru_replacer_test storage/lru_replacer_test.cpp rucbase::core unit lab1)
rucbase_add_gtest(lab1_buffer_pool_manager_test storage/buffer_pool_manager_test.cpp rucbase::core unit lab1)
rucbase_add_gtest(lab1_record_manager_test storage/record_manager_test.cpp rucbase::core unit lab1)
rucbase_add_gtest(lab1_record_types_test record/rm_record_test.cpp rucbase::core unit lab1)

rucbase_add_gtest(lab2_b_plus_tree_insert_test index/b_plus_tree_insert_test.cpp rucbase::core unit lab2)
rucbase_add_gtest(lab2_b_plus_tree_delete_test index/b_plus_tree_delete_test.cpp rucbase::core unit lab2)
rucbase_add_gtest(lab2_b_plus_tree_concurrent_test index/b_plus_tree_concurrent_test.cpp rucbase::core unit lab2)
rucbase_add_gtest(lab2_index_layout_test index/ix_layout_test.cpp rucbase::core unit lab2)
rucbase_add_gtest(lab2_index_handle_test index/ix_index_handle_test.cpp rucbase::core unit lab2)

rucbase_add_gtest(framework_catalog_metadata_test system/sm_meta_test.cpp rucbase::core unit framework)
rucbase_add_gtest(framework_wire_test net/wire_test.cpp rucbase::wire unit framework smoke)
rucbase_add_gtest(framework_parser_test parser/parser_test.cpp rucbase::parser unit framework smoke)
rucbase_add_gtest(framework_query_planning_test query/analyze_planner_test.cpp rucbase::core unit framework smoke lab3)
target_link_libraries(framework_query_planning_test PRIVATE rucbase::parser)

function(rucbase_add_blackbox_client target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE
            rucbase::wire
            rucbase::project_options)
endfunction()

rucbase_add_blackbox_client(blackbox_query_client query/query_test.cpp)
rucbase_add_blackbox_client(blackbox_transaction_client transaction/transaction_test.cpp)
rucbase_add_blackbox_client(blackbox_regress_client regress/regress_test_main.cpp regress/regress_test.cpp)
rucbase_add_blackbox_client(blackbox_concurrency_client
        concurrency/concurrency_test_main.cpp
        concurrency/concurrency_test.cpp
        regress/regress_test.cpp)

# The remaining C++ test programs are black-box clients. Pytest starts an rmdb
# process, gives each test its own database and port, then invokes these clients.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(RUCBASE_BLACKBOX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/blackbox")
    set(RUCBASE_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/test-logs")

    # Register black-box tests even when pytest is not installed. The fixture
    # turns a missing dependency into an explicit test failure instead of a
    # misleading successful run with zero discovered tests.
    rucbase_validate_test_labels("blackbox_pytest_dependency" blackbox)
    add_test(NAME blackbox_pytest_dependency
            COMMAND "${Python3_EXECUTABLE}" -c "import pytest")
    set_tests_properties(blackbox_pytest_dependency PROPERTIES
            LABELS blackbox
            FIXTURES_SETUP blackbox_dependencies
            TIMEOUT 10)

    function(rucbase_add_blackbox_test test_name)
        cmake_parse_arguments(TEST "" "" "LABELS;NODES" ${ARGN})
        if(TEST_UNPARSED_ARGUMENTS)
            message(FATAL_ERROR "${test_name} has unrecognized arguments: ${TEST_UNPARSED_ARGUMENTS}")
        endif()
        if(NOT TEST_LABELS)
            message(FATAL_ERROR "${test_name} must declare at least one CTest label")
        endif()
        if(NOT TEST_NODES)
            message(FATAL_ERROR "${test_name} must declare at least one pytest node")
        endif()
        rucbase_validate_test_labels("${test_name}" ${TEST_LABELS})

        set(pytest_nodes)
        foreach(node IN LISTS TEST_NODES)
            list(APPEND pytest_nodes "${RUCBASE_BLACKBOX_DIR}/${node}")
        endforeach()
        add_test(NAME "${test_name}"
                COMMAND "${Python3_EXECUTABLE}" -m pytest -q
                ${pytest_nodes}
                --rmdb-binary "$<TARGET_FILE:rmdb>"
                --query-client "$<TARGET_FILE:blackbox_query_client>"
                --transaction-client "$<TARGET_FILE:blackbox_transaction_client>"
                --regress-client "$<TARGET_FILE:blackbox_regress_client>"
                --concurrency-client "$<TARGET_FILE:blackbox_concurrency_client>"
                --log-dir "${RUCBASE_TEST_LOG_DIR}")
        set_tests_properties("${test_name}" PROPERTIES
                LABELS "${TEST_LABELS}"
                FIXTURES_REQUIRED blackbox_dependencies
                TIMEOUT 180)
    endfunction()

    # Handout-compatible "basic" suites (match transaction_test.py / concurrency_test.py).
    # Bonus cases are registered separately so students who only finished basics pass CTest.
    rucbase_add_blackbox_test(lab3_query_blackbox_test
            LABELS blackbox lab3 query-suite
            NODES "test_query.py::test_query")
    rucbase_add_blackbox_test(lab3_regress_blackbox_test
            LABELS blackbox lab3 regress-suite
            NODES "test_query.py::test_regress_client")
    rucbase_add_blackbox_test(lab4_transaction_blackbox_test
            LABELS blackbox lab4 transaction-suite
            NODES
            "test_transaction.py::test_transaction[commit_test]"
            "test_transaction.py::test_transaction[abort_test]")
    rucbase_add_blackbox_test(lab4_transaction_bonus_blackbox_test
            LABELS blackbox lab4 transaction-suite bonus
            NODES
            "test_transaction.py::test_transaction[commit_index_test]"
            "test_transaction.py::test_transaction[abort_index_test]")
    rucbase_add_blackbox_test(lab4_concurrency_blackbox_test
            LABELS blackbox lab4 concurrency-suite
            NODES
            "test_concurrency.py::test_concurrency[concurrency_read_test]"
            "test_concurrency.py::test_concurrency[dirty_write_test]"
            "test_concurrency.py::test_concurrency[dirty_read_test]"
            "test_concurrency.py::test_concurrency[lost_update_test]"
            "test_concurrency.py::test_concurrency[unrepeatable_read_test]"
            "test_concurrency.py::test_concurrency[unrepeatable_read_test_hard]")
    rucbase_add_blackbox_test(lab4_concurrency_bonus_blackbox_test
            LABELS blackbox lab4 concurrency-suite bonus
            NODES
            "test_concurrency.py::test_concurrency[phantom_read_test_1]"
            "test_concurrency.py::test_concurrency[phantom_read_test_2]"
            "test_concurrency.py::test_concurrency[phantom_read_test_3]"
            "test_concurrency.py::test_concurrency[phantom_read_test_4]")
else()
    message(STATUS "Python 3 was not found; black-box tests are not registered")
endif()
