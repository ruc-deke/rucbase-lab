#!/bin/bash
rm -r ExecutorTest_db
rm output.txt
cat input_all_task.sql | while read line
do
    if [ ${#line} -eq 0 ] || [ ${line:0:1} == "#" ]
    then
        echo "$line"
        continue
    fi
    echo ">> $line"
    ../../build/bin/exec_sql "$line"
    echo "------------------------------"
done | tee -a output.txt
echo "check different"
diff res_taskall_output.txt output.txt
if [ $? != 0 ]
    then
        echo "Pass Failed!"
    else
        echo "Pass Success!"
fi
rm -r ExecutorTest_db