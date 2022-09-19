# replacer module
set(SOURCES lru_replacer.cpp clock_replacer.cpp)
add_library(lru_replacer STATIC ${SOURCES})
add_library(clock_replacer STATIC ${SOURCES})

add_executable(lru_replacer_test lru_replacer_test.cpp)
target_link_libraries(lru_replacer_test lru_replacer gtest_main)  # add gtest



add_executable(clock_replacer_test clock_replacer_test.cpp)
target_link_libraries(clock_replacer_test clock_replacer gtest_main)  # add gtest


