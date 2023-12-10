import os
import sys
import time

NUM_TESTS = 10

TESTS = {
    "concurrency_read_test": {"check_method": "dict_match", "score": 10},
    "dirty_write_test": {"check_method": "dict_match", "score": 10},
    "dirty_read_test": {"check_method": "dict_match", "score": 10},
    "lost_update_test": {"check_method": "dict_match", "score": 10},
    "unrepeatable_read_test": {"check_method": "dict_match", "score": 10},
    "unrepeatable_read_test_hard": {"check_method": "dict_match", "score": 10},
    "phantom_read_test_1": {"check_method": "diff_match", "score": 5},
    "phantom_read_test_2": {"check_method": "diff_match", "score": 5},
    "phantom_read_test_3": {"check_method": "diff_match", "score": 5},
    "phantom_read_test_4": {"check_method": "diff_match", "score": 5}
}


FAILED_TESTS = []

def get_test_name(test_name):
    return "../src/test/concurrency/concurrency_sql/" + str(test_name) + ".sql"

def get_output_name(test_name):
    return "../src/test/concurrency/concurrency_sql/" + str(test_name) + "_output.txt"

def build():
    # root
    os.chdir("../../../")
    if os.path.exists("./build"):
        os.system("rm -rf build")
    os.mkdir("./build")
    os.chdir("./build")
    os.system("cmake ..")
    os.system("make rmdb -j4")
    os.system("make concurrency_test -j4")
    os.chdir("..")

def run_test(test_case):
    if test_case not in TESTS:
        print(f"Test case '{test_case}' is not recognized.")
        return
    os.chdir("./build")
    score = 0.0
    index = -1
    comment_str = "\n"

    print("-----------Concurrency Unit Testing " + test_case + "...-----------")
    test_file = get_test_name(test_case)
    database_name = "concurrency_test_db"

    if os.path.exists(database_name):
        os.system("rm -rf " + database_name)

    os.system("./bin/rmdb " + database_name + " &")
    # ./bin/concurrency ../src/test/concurrency_test/xxx.sql
    # The server takes a few seconds to establish the connection, so the client should wait for a while.
    time.sleep(3)
    os.system("./bin/concurrency_test " + test_file + " " + database_name + "/client_output.txt")

    # check result 
    test_info = TESTS[test_case]
    check_method = test_info["check_method"]
    test_score = test_info["score"]
    if check_method == "dict_match":
        ansDict = {}
        standard_answer = get_output_name(test_case)
        hand0 = open(standard_answer, "r")
        for line in hand0:
            line = line.strip('\n')
            if line == "":
                continue
            num = ansDict.setdefault(line, 0)
            ansDict[line] = num + 1
        my_answer = database_name + "/client_output.txt"
        hand1 = open(my_answer, "r")
        for line in hand1:
            line = line.strip('\n')
            if line == "":
                continue
            num = ansDict.setdefault(line, 0)
            ansDict[line] = num - 1
        match = True
        for key, value in ansDict.items():
            if value != 0:
                match = False
                comment_str += "In " + test_case + ", your answer mismatches standard answer.\n"
                break
        if match:
            score += test_score
        else:
            FAILED_TESTS.append(test_case)
    else:
        res = os.system("diff " + database_name + "/client_output.txt " + get_output_name(test_case) + " -w")
        print("diff result: " + str(res))
        # calculate the score
        if res == 0:
            score += test_score
        else:
            FAILED_TESTS.append(test_case)
            comment_str += "In " + test_case + ", your answer mismatches standard answer.\n"

    # close server
    os.system("ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9")
    print("finish kill")
    # delete database
    # os.system("rm -rf ./" + database_name)
    # print("finish delete database")
    
    os.chdir("../../")
    

    if(len(FAILED_TESTS) != 0):
        print(f"\033[0;31;40mConcurrency Unit Test {test_case} Final Score:" + str(score)+ "\033[0m")
        print("Your program fails the following test cases: ")
        for failed_test in FAILED_TESTS:
            print("[" + failed_test + "]  "),
    else:
        print(f"You have passed unit test case {test_case} about concurrency control.")
        print(f"\033[0;31;40mConcurrency Unit Test {test_case} Final Score:" + str(score)+ "\033[0m")

    print(comment_str)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 concurrency_unit_test.py <test_case_name>")
        sys.exit(1)

    build()
    test_case_name = sys.argv[1]
    run_test(test_case_name)
