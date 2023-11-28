import os;
import time;
import sys;
# test : basic_query
NUM_TESTS = 5
SCORES = [25, 15, 15, 15, 30]

# current dir is root/build
def get_test_name(index):
    return "../src/test/query/query_sql/basic_query_test"+str(index)+".sql"

def get_output_name(index):
    return "../src/test/query/query_sql/basic_query_answer"+str(index)+".txt"

def extract_index(test_file):
    # Extract the index from the test file name
    index_str = ''.join(c for c in test_file if c.isdigit())
    if index_str:
        return int(index_str)
    else:
        print(f"Error: Could not extract index from the test file {test_file}.")
        sys.exit(1)

def build():
    # change dir to root
    os.chdir("../../../")
    if os.path.exists("./build"):
        os.system("rm -rf build")
    os.mkdir("./build")
    os.chdir("./build")
    os.system("cmake ..")
    os.system("make rmdb -j4")
    os.system("make query_test -j4")
    os.chdir("..")
    

def run(test_file):
    # dir is root/build
    os.chdir("./build")
    score = 0.0
    
    test_index = extract_index(test_file)

    if not (1 <= test_index <= NUM_TESTS):
        print("Error: Invalid index. The index should be between 1 and {NUM_TEST}.")
        exit(0)

    test_file = get_test_name(test_index)
    database_name = "query_test_db"

    if os.path.exists(database_name):
        os.system("rm -rf " + database_name)

    os.system("./bin/rmdb " + database_name + "&")
    # The server takes a few seconds to establish the connection, so the client should wait for a while.
    time.sleep(3)
    ret = os.system("./bin/query_test " + test_file)
    if(ret != 0):
        print("Error. Stopping")
        exit(0)
    
    # check result 
    ansDict={}
    standard_answer = get_output_name(test_index)
    hand0 = open(standard_answer,"r")
    for line in hand0 :
        line = line.strip('\n')
        if line == "":
            continue
        num=ansDict.setdefault(line,0)
        ansDict[line]=num+1
    my_answer = database_name + "/output.txt"
    hand1 = open(my_answer,"r")
    for line in hand1 :
        line = line.strip('\n')
        if line == "":
            continue
        num=ansDict.setdefault(line,0)
        ansDict[line]=num-1
    match = True
    for key,value in ansDict.items():
        if value != 0:
            match = False
            if value > 0:
                print('In basic query test'+str(test_index),'Mismatch,your answer lack items')
            else :
                print('In basic query test'+str(test_index),'Mismatch,your answer has redundant items')
    if match :
        score += SCORES[test_index-1]
    # close server
    os.system("ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9")
    print("finish kill")
    # delete database
    if test_index < 5:
        os.system("rm -rf ./" + database_name)
        print("finish delete database")
    
    os.chdir("../../")
    print("Unit Test Score: " + str(score))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 query_unit_test.py <test_name>")
        exit(0)
    
    build()
    run(sys.argv[1])