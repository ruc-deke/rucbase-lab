// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "net/client.h"

class Operation {
public:
    std::string name;  // 比如t1a, t1b, t2a
    std::string sql;
    int txn_id = -1;
};

class OperationPermutation {
public:
    std::vector<Operation*> operations;
};

class Transaction {
public:
    std::vector<std::unique_ptr<Operation>> operations;
    int txn_id = -1;
    rucbase::wire::Client client;
};

class TestCaseAnalyzer {
public:
    void analyze_operation(Operation* operation, const std::string& operation_line);
    void analyze_test_case();

    OperationPermutation permutation;
    std::vector<std::unique_ptr<Transaction>> transactions;
    std::vector<std::string> preload;
    std::string infile_path;
    std::fstream infile;
    std::unordered_map<std::string, Operation*> operation_map;
};
