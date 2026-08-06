add_library(rucbase_wire STATIC wire.cpp)
target_include_directories(rucbase_wire PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(rucbase_wire PUBLIC cxx_std_17)
rucbase_configure_target(rucbase_wire)
