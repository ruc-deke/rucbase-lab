preload 4
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);

txn1 5
t1a begin;
t1b select * from concurrency_test where id = 1;
t1c update concurrency_test set score = 100.0 where id = 1;
t1d commit;
t1e select * from concurrency_test where id = 1;

txn2 3
t2a begin;
t2b update concurrency_test set score = 60.0 where id = 1;
t2c commit;

permutation 7
t1a
t2a
t1b
t1c
t2b
t1d
t1e
