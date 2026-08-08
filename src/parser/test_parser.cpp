// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0
#undef NDEBUG

#include <cassert>

#include "ast_printer.h"
#include "parser.h"

int main() {
    std::vector<std::string> sqls = {
        "show tables;",
        "desc tb;",
        "create table tb (a int, b float, c char(4));",
        "drop table tb;",
        "create index tb(a);",
        "create index tb(a, b, c);",
        "drop index tb(a, b, c);",
        "drop index tb(b);",
        "insert into tb values (1, 3.14, 'pi');",
        "delete from tb where a = 1;",
        "update tb set a = 1, b = 2.2, c = 'xyz' where x = 2 and y < 1.1 and z > 'abc';",
        "select * from tb;",
        "select * from tb where x <> 2 and y >= 3. and z <= '123' and b < tb.a;",
        "select x.a, y.b from x, y where x.a = y.b and c = d;",
        "select x.a, y.b from x join y where x.a = y.b and c = d;",
        "exit;",
        "help;",
        "",
    };
    for (auto &sql : sqls) {
        std::cout << sql << std::endl;
        rucbase::parser::ResetParseError();
        YY_BUFFER_STATE buf = yy_scan_string(sql.c_str());
        assert(yyparse() == 0);
        yy_delete_buffer(buf);
        if (ast::parse_tree != nullptr) {
            ast::TreePrinter::print(ast::parse_tree);
            std::cout << std::endl;
        } else {
            std::cout << "exit/EOF" << std::endl;
        }
    }

    const std::string invalid_sql = "show tab les;";
    rucbase::parser::ResetParseError();
    YY_BUFFER_STATE buf = yy_scan_string(invalid_sql.c_str());
    assert(yyparse() != 0);
    yy_delete_buffer(buf);

    const rucbase::parser::ParseError error = rucbase::parser::GetParseError();
    assert(error.line == 1);
    assert(error.column == 6);
    assert(error.message.find("unexpected IDENTIFIER") != std::string::npos);
    assert(error.message.find("expecting TABLES") != std::string::npos);

    const std::string diagnostic = rucbase::parser::FormatParseError(invalid_sql, error);
    assert(diagnostic.find("Parser Error at line 1 column 6") != std::string::npos);
    assert(diagnostic.find("\nshow tab les;\n     ^\n") != std::string::npos);

    const std::string invalid_character_sql = "show tables；;";
    rucbase::parser::ResetParseError();
    buf = yy_scan_string(invalid_character_sql.c_str());
    assert(yyparse() != 0);
    yy_delete_buffer(buf);

    const rucbase::parser::ParseError invalid_character_error = rucbase::parser::GetParseError();
    assert(invalid_character_error.line == 1);
    assert(invalid_character_error.column == 12);
    assert(invalid_character_error.message.find("unexpected INVALID") != std::string::npos);

    const std::string invalid_character_diagnostic =
        rucbase::parser::FormatParseError(invalid_character_sql, invalid_character_error);
    assert(invalid_character_diagnostic.find("Parser Error at line 1 column 12") != std::string::npos);
    assert(invalid_character_diagnostic.find("\nshow tables；;\n           ^\n") != std::string::npos);

    ast::parse_tree.reset();
    return 0;
}
