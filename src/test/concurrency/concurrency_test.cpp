#include "concurrency_test.h"
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

void TestCaseAnalyzer::analyze_operation(Operation* operation, const std::string& operation_line) {
    operation->name = operation_line.substr(0, operation_line.find(" "));
    operation->sql = operation_line.substr(operation_line.find(" ") + 1);
}

void TestCaseAnalyzer::analyze_test_case() {
    std::string line;

    infile.open(infile_path);

    Operation* crash_operation = new Operation();
    crash_operation->sql = "crash";
    crash_operation->txn_id = -1;

    while(std::getline(infile, line)) {
        if(line.find("preload") != std::string::npos) {
            int count = atoi(line.substr(line.find(" ") + 1).c_str());
            while(count) {
                --count;
                std::getline(infile, line);
                preload.push_back(line);
            }
        }
        else if(line.find("txn") != std::string::npos) {
            Transaction* txn = new Transaction();

            transactions.push_back(txn);
            txn->txn_id = transactions.size() - 1;

            int count = atoi(line.substr(line.find(" ") + 1).c_str());
            while(count) {
                --count;
                std::getline(infile, line);
                Operation* operation = new Operation();
                txn->operations.push_back(operation);
                analyze_operation(operation, line);
                operation->txn_id = txn->txn_id;
                operation_map[operation->name] = operation;
            }
        }
        else if(line.find("permutation") != std::string::npos) {
            int count = atoi(line.substr(line.find(" ") + 1).c_str());
            while(count) {
                --count;
                std::getline(infile, line);
                if(strcmp(line.c_str(), "crash") == 0) {
                    // permutation->operations.push_back(crash_operation);
                    break;
                }
                else {
                    permutation->operations.push_back(operation_map[line]);
                }
            }
        }
    }
}