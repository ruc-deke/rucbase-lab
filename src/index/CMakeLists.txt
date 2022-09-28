# index module
set(SOURCES ix_index_handle.cpp ix_scan.cpp ../common/rwlatch.cpp)
add_library(index STATIC ${SOURCES})
target_link_libraries(index storage)
