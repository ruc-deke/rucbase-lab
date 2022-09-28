# record module
set(SOURCES rm_file_handle.cpp rm_scan.cpp)
add_library(record STATIC ${SOURCES})
add_library(records SHARED ${SOURCES})
target_link_libraries(record storage system transaction)

# rm_gtest
add_executable(rm_gtest rm_gtest.cpp)
target_link_libraries(rm_gtest record gtest_main)