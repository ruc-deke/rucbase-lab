preload 4
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);

txn1 4
t1a begin;
t1b select * from concurrency_test;
t1c select * from concurrency_test where id = 2;
t1d commit;

txn2 4
t2a begin;
t2b select * from concurrency_test;
t2c select * from concurrency_test where id = 3;
t2d commit;

permutation 8
t1a
t2a
t1b
t2b
t2c
t1c
t1d
t2d