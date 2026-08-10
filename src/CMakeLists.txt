# 先定义 SQL 解析器和网络协议库，供后续内核、服务端及测试目标复用。
add_subdirectory(parser)
add_subdirectory(net)

# 教学内核各模块联系紧密，统一构建为一个静态库可以避免按目录拆分时产生循环依赖或重复链接。
set(RUCBASE_CORE_SOURCES
        analyze/analyzer.cpp
        execution/execution_manager.cpp
        index/b_plus_tree.cpp
        index/index_manager.cpp
        index/index_scan.cpp
        index/index_types.cpp
        optimizer/planner.cpp
        portal/portal.cpp
        record/rm_file_handle.cpp
        record/rm_scan.cpp
        recovery/log_manager.cpp
        recovery/log_recovery.cpp
        replacer/lru_replacer.cpp
        storage/buffer_pool_manager.cpp
        storage/disk_manager.cpp
        system/sm_meta.cpp
        system/sm_manager.cpp
        transaction/concurrency/lock_manager.cpp
        transaction/transaction_manager.cpp)

# 为项目内部目标提供带命名空间的稳定别名，避免直接依赖具体库名。
add_library(rucbase_core STATIC ${RUCBASE_CORE_SOURCES})
add_library(rucbase::core ALIAS rucbase_core)

# src 下的公共头文件需要传递给依赖目标，deps 仅供内核自身编译时使用。
target_include_directories(rucbase_core PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
        PRIVATE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/deps>")

# 线程库属于内核的公共使用要求；警告、标准和 Sanitizer 等项目选项无需向外传播。
target_link_libraries(rucbase_core
        PUBLIC Threads::Threads
        PRIVATE rucbase::project_options)

# rmdb 组合服务端入口、教学内核、解析器和通信协议，生成数据库服务端程序。
add_executable(rmdb rmdb.cpp server/server.cpp)
target_link_libraries(rmdb PRIVATE
        rucbase::core
        rucbase::parser
        rucbase::wire
        rucbase::project_options
        Threads::Threads)

# 仅在顶层启用 CTest 时注册测试目标，便于裁剪非测试构建。
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
