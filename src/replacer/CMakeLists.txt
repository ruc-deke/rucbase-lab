set(SOURCES lru_replacer.cpp)
add_library(lru_replacer STATIC ${SOURCES})
rucbase_configure_target(lru_replacer)
