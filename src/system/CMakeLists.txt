set(SOURCES sm_manager.cpp)
add_library(system STATIC ${SOURCES})
target_link_libraries(system index record)


# sm_gtest
add_executable(sm_gtest sm_gtest.cpp)
target_link_libraries(sm_gtest system gtest_main)
