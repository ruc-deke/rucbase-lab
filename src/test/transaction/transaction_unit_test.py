import os
import sys
import time
import subprocess

NUM_TESTS = 1

TESTS = ["commit_test",
         "abort_test",
         "commit_index_test",
         "abort_index_test"]

FAILED_TESTS = []

def get_test_name(test_case):
    return "../src/test/transaction/transaction_sql/" + str(test_case) + ".sql"

def get_output_name(test_case):
    return "../src/test/transaction/transaction_sql/" + str(test_case) + "_output.txt"

def build():
    # root
    os.chdir("../../../")
    if os.path.exists("./build"):
        os.system("rm -rf build")
    os.mkdir("./build")
    os.chdir("./build")
    os.system("cmake ..")
    os.system("make rmdb -j4")
    os.system("make transaction_test -j4")
    os.chdir("..")

def run_test(test_case):
    if test_case not in TESTS:
        print(f"Test case '{test_case}' is not recognized.")
        return
    os.chdir("./build")
    score = 0.0
    comment_str = "\n"

    print("-----------Transaction Unit Testing " + test_case + "...-----------")
    test_file = get_test_name(test_case)
    database_name = "transaction_test_db"

    if os.path.exists(database_name):
        os.system("rm -rf " + database_name)

    os.system("./bin/rmdb " + database_name + " &")
    # ./bin/transaction_test ../src/test/transaction_test/commit_test.sql
    # The server takes a few seconds to establish the connection, so the client should wait for a while.
    time.sleep(3)
    ret = os.system("./bin/transaction_test " + test_file)
    if(ret != 0):
        print("Error. Stopping")
        exit(0)
    # check result 
    ansDict = {}
    standard_answer = get_output_name(test_case)
    my_answer = database_name+"/output.txt"
    try:
        hand0 = open(standard_answer, "r")
    except:
        FAILED_TESTS.append(test_case)
        comment_str += ("In Transaction unit test:" +
                        str(test_case) + ", open standard_answer failed\n")
        subprocess.run(
            "ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9", shell=True)
        return
    else:
        for line in hand0:
            line = line.strip('\n')
            if line == "":
                continue
            num = ansDict.setdefault(line, 0)
            ansDict[line] = num+1
    # try to get your answer
    try:
        hand1 = open(my_answer, "r")
    except:
        FAILED_TESTS.append(test_case)
        comment_str += ("In Transaction unit test:" +
                        str(test_case) + ", cannot open output file\n")
        subprocess.run(
            "ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9", shell=True)
        return
    else:
        for line in hand1:
            line = line.strip('\n')
            if line == "":
                continue
            num = ansDict.setdefault(line, 0)
            ansDict[line] = num-1
    # Dict match
    match = True
    for key, value in ansDict.items():
        if value != 0:
            match = False
            comment_str += "In Transaction unit test:" + \
                str(test_case)+", your answer mismatches standard answer\n"
            break
    if match:
        score += 20
    else:
        FAILED_TESTS.append(test_case)

    # close server
    os.system("ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9")
    print("finish kill")
    
    os.chdir("../../")
    if (len(FAILED_TESTS) != 0):
        print(f"\033[0;31;40mTransaction Test {test_case} Final Score:" + str(score)+ "\033[0m")
        print("Your program fails the following test cases: ")
        for failed_test in FAILED_TESTS:
            print("[" + failed_test + "]  "),
    else:
        print(f"You have passed unit test case {test_case} about transaction!")
        print(f"\033[0;31;40mTransaction Test {test_case} Final Score:" + str(score)+ "\033[0m")
    print(comment_str)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 transaction_unit_test.py <test_case_name>")
        sys.exit(1)

    build()
    test_case_name = sys.argv[1]
    run_test(test_case_name)