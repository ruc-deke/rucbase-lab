preload 10
create table concurrency_test (id int, name char(8), score float);
create index concurrency_test (id);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (4, 'zhanghua', 88.5);
insert into concurrency_test values (7, 'xiaoyang', 91.0);
insert into concurrency_test values (10, 'wangming', 92.0);
insert into concurrency_test values (8, 'wanghong', 93.0);
insert into concurrency_test values (100, 'zhaoming', 94.0);
insert into concurrency_test values (201, 'zhaohong', 95.0);

txn1 6
t1a begin;
t1b select * from concurrency_test where id > 2 and id < 10;
t1c select * from concurrency_test where id > 4 and id < 20;
t1d select * from concurrency_test where id > 9 and id < 200;
t1e select * from concurrency_test where id > 9 and id < 100;
t1f commit;

txn2 3
t2a begin;
t2b delete from concurrency_test where id = 7;
t2c abort;

txn3 3
t3a begin;
t3b insert into concurrency_test values (11, 'zhaoyang', 99.0);
t3c abort;

txn4 3
t4a begin;
t4b update concurrency_test set id = 13 where name = 'wanghong';
t4c abort;

permutation 12
t1a
t2a
t3a
t4a
t1b
t2b
t1c
t3b
t1d
t4b
t1e
t1f