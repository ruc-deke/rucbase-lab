import os;
import time;
# test : basic_query
NUM_TESTS = 5
SCORES = [25, 15, 15, 15, 30]

# current dir is root/build
def get_test_name(index):
    return "../src/test/query/query_sql/basic_query_test"+str(index)+".sql"

def get_output_name(index):
    return "../src/test/query/query_sql/basic_query_answer"+str(index)+".txt"

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
    

def run():
    # dir is root/build
    os.chdir("./build")
    score = 0.0
    
    for i in range(NUM_TESTS):
        # if i == 0 :
        #     continue
        
        test_file = get_test_name(i + 1)
        database_name = "query_test_db"

        if os.path.exists(database_name):
            os.system("rm -rf " + database_name)

        os.system("./bin/rmdb " + database_name + "&")
        # ./bin/query_test ../src/test/query_test/aggregate_test.sql
        # The server takes a few seconds to establish the connection, so the client should wait for a while.
        time.sleep(3)
        ret = os.system("./bin/query_test " + test_file)
        if(ret != 0):
            print("Error. Stopping")
            exit(0)
        
        # check result 
        ansDict={}
        standard_answer = get_output_name(i + 1)
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
                    print('In basic query test'+str(i+1),'Mismatch,your answer lack items')
                else :
                    print('In basic query test'+str(i+1),'Mismatch,your answer has redundant items')
        if match :
            score += SCORES[i]
        # close server
        os.system("ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9")
        print("finish kill")
        # delete database
        if i < 4:
            os.system("rm -rf ./" + database_name)
            print("finish delete database")
    
    os.chdir("../../")
    print("final score: " + str(score))

if __name__ == "__main__":
    build()
    run()
