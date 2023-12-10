preload 4
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);

txn1 4
t1a begin;
t1b select * from concurrency_test where id = 2;
t1c update concurrency_test set score = 100.0 where id = 2;
t1d commit;

txn2 5
t2a begin;
t2b select * from concurrency_test where id = 2;
t2c update concurrency_test set score = 75.5 where id = 2;
t2d commit;
t2e select * from concurrency_test where id = 2;

permutation 8
t1a
t2a
t2b
t1b
t1c
t2c
t2d
t2e