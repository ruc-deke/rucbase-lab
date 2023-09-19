set(SOURCES rm_file_handle.cpp rm_scan.cpp)
add_library(record STATIC ${SOURCES})
add_library(records SHARED ${SOURCES})
target_link_libraries(record system transaction system storage)
