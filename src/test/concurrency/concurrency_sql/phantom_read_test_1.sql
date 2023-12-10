preload 5
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (4, 'zhanghua', 88.5);
create index concurrency_test (id);

txn1 4
t1a begin;
t1b select * from concurrency_test;
t1c select * from concurrency_test;
t1d commit;

txn2 3
t2a begin;
t2b insert into concurrency_test values (3, 'xiaoyang', 100.0);
t2c abort;

permutation 6
t1a
t2a
t1b
t2b
t1c
t1d