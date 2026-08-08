find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(RUCBASE_PARSER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${RUCBASE_PARSER_GENERATED_DIR}")

bison_target(rucbase_yacc yacc.y "${RUCBASE_PARSER_GENERATED_DIR}/yacc.tab.cpp"
        DEFINES_FILE "${RUCBASE_PARSER_GENERATED_DIR}/yacc.tab.h")
flex_target(rucbase_lex lex.l "${RUCBASE_PARSER_GENERATED_DIR}/lex.yy.cpp")
add_flex_bison_dependency(rucbase_lex rucbase_yacc)

add_library(rucbase_parser STATIC
        parser.cpp
        ${BISON_rucbase_yacc_OUTPUT_SOURCE}
        ${FLEX_rucbase_lex_OUTPUTS})
add_library(rucbase::parser ALIAS rucbase_parser)
target_include_directories(rucbase_parser
        PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${RUCBASE_PARSER_GENERATED_DIR}")
target_link_libraries(rucbase_parser PRIVATE rucbase::project_options)

# Bison-generated yacc.tab.cpp sets yynerrs without reading it under our options.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set_source_files_properties(${BISON_rucbase_yacc_OUTPUT_SOURCE} PROPERTIES
            COMPILE_OPTIONS "-Wno-unused-but-set-variable")
    # Flex compares its unsigned buffer size with an internal signed counter.
    set_source_files_properties(${FLEX_rucbase_lex_OUTPUTS} PROPERTIES
            COMPILE_OPTIONS "-Wno-sign-compare")
endif()
