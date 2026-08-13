// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "concurrency_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <charconv>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

/**
 * @brief 从测试用例的“标签 数量”行读取非负数量。
 * @throws std::runtime_error 输入缺少数量或数量不是合法整数。
 */
int ParseCount(const std::string& line) {
    const auto separator = line.find(' ');
    if (separator == std::string::npos) {
        throw std::runtime_error("missing operation count: " + line);
    }

    int count = 0;
    const char* begin = line.data() + separator + 1;
    const char* end = line.data() + line.size();
    const auto result = std::from_chars(begin, end, count);
    if (result.ec != std::errc{} || result.ptr != end || count < 0) {
        throw std::runtime_error("invalid operation count: " + line);
    }
    return count;
}

}  // namespace

void TestCaseAnalyzer::analyze_operation(Operation* operation, const std::string& operation_line) {
    operation->name = operation_line.substr(0, operation_line.find(' '));
    operation->sql = operation_line.substr(operation_line.find(' ') + 1);
}

void TestCaseAnalyzer::analyze_test_case() {
    std::string line;

    infile.open(infile_path);

    while (std::getline(infile, line)) {
        if (line.find("preload") != std::string::npos) {
            int count = ParseCount(line);
            while (count) {
                --count;
                std::getline(infile, line);
                preload.push_back(line);
            }
        } else if (line.find("txn") != std::string::npos) {
            auto txn = std::make_unique<Transaction>();
            txn->txn_id = static_cast<int>(transactions.size());
            Transaction* const transaction = txn.get();
            transactions.push_back(std::move(txn));

            int count = ParseCount(line);
            while (count) {
                --count;
                std::getline(infile, line);
                auto operation = std::make_unique<Operation>();
                Operation* const operation_ptr = operation.get();
                analyze_operation(operation_ptr, line);
                operation_ptr->txn_id = transaction->txn_id;
                operation_map[operation_ptr->name] = operation_ptr;
                transaction->operations.push_back(std::move(operation));
            }
        } else if (line.find("permutation") != std::string::npos) {
            int count = ParseCount(line);
            while (count) {
                --count;
                std::getline(infile, line);
                if (strcmp(line.c_str(), "crash") == 0) {
                    break;
                } else {
                    permutation.operations.push_back(operation_map[line]);
                }
            }
        }
    }
}
