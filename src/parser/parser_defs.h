// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <string>
#include <string_view>

#include "defs.h"

namespace rucbase::parser {

struct ParseError {
    int line = 1;
    int column = 1;
    std::string message;
};

void ResetParseError();
void RecordParseError(int line, int column, const char* message);
ParseError GetParseError();
std::string FormatParseError(std::string_view sql, const ParseError& error);

}  // namespace rucbase::parser

int yyparse();

typedef struct yy_buffer_state *YY_BUFFER_STATE;

YY_BUFFER_STATE yy_scan_string(const char *str);

void yy_delete_buffer(YY_BUFFER_STATE buffer);
