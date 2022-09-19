include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(record)
add_subdirectory(index)
add_subdirectory(system)
add_subdirectory(execution)
add_subdirectory(parser)
add_subdirectory(storage)
add_subdirectory(common)
add_subdirectory(replacer)
add_subdirectory(transaction)
add_subdirectory(recovery)

# 后续lab开放
# add_executable(rawcli rawcli.cpp)
# target_link_libraries(rawcli parser execution pthread)

# add_executable(rucbase rucbase.cpp)
# target_link_libraries(rucbase parser execution readline pthread)

# add_library(ownbase STATIC ownbase.cpp)