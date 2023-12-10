#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

class Operation {
public:
    std::string name;   // 比如t1a, t1b, t2a
    std::string sql;    
    int txn_id;
};

class OperationPermutation {
public:
    std::vector<Operation*> operations;
};

class Transaction {
public:
    std::vector<Operation*> operations;
    int txn_id;
    int sockfd;
};

class TestCaseAnalyzer {
public:
    TestCaseAnalyzer() {
        permutation = new OperationPermutation();
    }
    void analyze_operation(Operation* operation, const std::string& operation_line);
    void analyze_test_case();

    OperationPermutation* permutation;
    std::vector<Transaction*> transactions;
    std::vector<std::string> preload;
    std::string infile_path;
    std::fstream infile;
    std::unordered_map<std::string, Operation*> operation_map;
};

