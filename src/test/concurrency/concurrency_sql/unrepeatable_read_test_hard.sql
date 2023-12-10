preload 4
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (4, 'zhanghua', 88.5);

txn1 4
t1a begin;
t1b select * from concurrency_test where name = 'xiaohong';
t1c select * from concurrency_test where name = 'xiaohong';
t1d commit;

txn2 3
t2a begin;
t2b update concurrency_test set name = 'xiaohong' where id = 2;
t2c abort;

permutation 6
t1a
t2a
t1b
t2b
t1c
t1d
