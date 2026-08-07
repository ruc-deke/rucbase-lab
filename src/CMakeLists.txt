add_subdirectory(parser)
add_subdirectory(net)

# The teaching kernel is one deployable component with tightly coupled modules.
# Building it as one archive avoids circular and duplicate per-directory libraries.
set(RUCBASE_CORE_SOURCES
        analyze/analyze.cpp
        execution/execution_manager.cpp
        index/ix_index_handle.cpp
        index/ix_scan.cpp
        optimizer/planner.cpp
        record/rm_file_handle.cpp
        record/rm_scan.cpp
        recovery/log_manager.cpp
        recovery/log_recovery.cpp
        replacer/lru_replacer.cpp
        storage/buffer_pool_manager.cpp
        storage/disk_manager.cpp
        system/sm_manager.cpp
        transaction/concurrency/lock_manager.cpp
        transaction/transaction_manager.cpp)

add_library(rucbase_core STATIC ${RUCBASE_CORE_SOURCES})
add_library(rucbase::core ALIAS rucbase_core)
target_include_directories(rucbase_core PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(rucbase_core
        PUBLIC Threads::Threads
        PRIVATE rucbase::project_options)

add_executable(rmdb rmdb.cpp server/server.cpp)
target_link_libraries(rmdb PRIVATE
        rucbase::core
        rucbase::parser
        rucbase::wire
        rucbase::project_options
        Threads::Threads)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
