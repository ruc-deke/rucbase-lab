find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(yacc yacc.y ${CMAKE_CURRENT_BINARY_DIR}/yacc.tab.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/yacc.tab.h)
flex_target(lex lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)
add_flex_bison_dependency(lex yacc)

set(SOURCES ${BISON_yacc_OUTPUT_SOURCE} ${FLEX_lex_OUTPUTS} ast.cpp)
add_library(parser STATIC ${SOURCES})
target_include_directories(parser PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR})
rucbase_configure_target(parser)
# Bison-generated yacc.tab.cpp sets yynerrs without reading it under our options.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set_source_files_properties(${BISON_yacc_OUTPUT_SOURCE} PROPERTIES
            COMPILE_OPTIONS "-Wno-unused-but-set-variable")
endif()

if(BUILD_TESTING)
    add_executable(test_parser test_parser.cpp)
    target_link_libraries(test_parser PRIVATE parser)
    rucbase_configure_target(test_parser)
    add_test(NAME test_parser COMMAND test_parser
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set_tests_properties(test_parser PROPERTIES LABELS unit)
endif()
