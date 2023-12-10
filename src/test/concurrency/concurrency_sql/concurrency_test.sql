-- 题目九：事务控制语句
-- 测试点1: 并发读
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);
-- thread1:
begin;
select * from concurrency_test;
commit;
-- thread2:
begin;
select * from concurrency_test;
commit;

-- 测试点2: 脏写数据异常
-- W1(x)W2(x)C1C2
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);
-- t1:
begin;
-- t2:
begin;
-- t1
select * from concurrency_test where id = 1;
update concurrency_test set score = 100.0 where id = 1;
-- t2
update concurrency_test set score = 60.0 where id = 1;
-- 由于是no-wait，所以应该abort t2
-- t1
commit;
select * from concurrency_test;

-- 测试点3: 脏读数据异常
-- W1(x)R2(x)A1
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);
-- t1
begin;
-- t2
begin;
-- t1
select * from concurrency_test where id = 1;
update concurrency_test set score = 100.0 where id = 1;
-- t2
select * from concurrency_test where id = 1;
-- 由于是no-wait，所以应该abort t2
-- t1
commit;
select * from concurrency_test;

-- 测试点4: 丢失修改数据异常
-- R1(x)R2(x)W1(x)C1W2(x)C2
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);
-- t1
begin;
-- t2;
begin;
select * from concurrency_test where id = 1;
-- t1
select * from concurrency_test where id = 1;
update concurrency_test set score = 100.0 where id = 1;
-- 由于是no-wait，所以应该abort t1
-- t2
update concurrency_test set score = 75.5 where id = 1;
commit;
select * from concurrency_test;

-- 测试点5: 不可重复读数据异常
-- R1(x)W2(x)C2R1(x)
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (3, 'zhanghua', 88.5);
-- t1
begin;
-- t2
begin;
-- t1
select * from concurrency_test where id = 2;
-- t2
update concurrency_test set score = 100.0 where id = 2;
-- 由于是no-wait，所以应该abort t2
-- t1
select * from concurrency_test where id = 2;
commit;
select * from concurrency_test;

-- 测试点6: 幻读数据异常
create table concurrency_test (id int, name char(8), score float);
insert into concurrency_test values (1, 'xiaohong', 90.0);
insert into concurrency_test values (2, 'xiaoming', 95.0);
insert into concurrency_test values (4, 'zhanghua', 88.5);
create index concurrency_test (id);
-- t1
begin;
select * from concurrency_test;
-- t2
begin;
insert into concurrency_test values (5, 'xiaoyang', 100.0);
-- 如果是加表级锁，根据no-wait规则，应该abort t2
-- 如果是加间隙锁，这里应该是什么操作
-- t1
select * from concurrency_test;
commit;
-- t2，如果t2没有abort就应该执行下面这条语句，否则就不执行
abort;
-- t3
begin;
select * from concurrency_test;
-- t4
begin;
insert into concurrency_test value (3, 'xiaoyang', 100.0);
-- 如果是加表级锁，根据no-wait规则，应该abort t2
-- 如果是加间隙锁，这里应该是什么操作
-- t3
select * from concurrency_test;
commit;
-- t4 如果t4没有abort则执行下面的操作，否则不执行
abort;
select * from concurrency_test;

-- 由于不确定到底采用什么样的策略去处理间隙锁的死锁问题，因此可以只对t1和t3的select结果做检查
-- mysql是采用死锁检测或者超时回滚的方法来处理间隙锁带来的死锁

preload 3
create ...;
insert ...;
insert ...;

txn1 3
t1a begin;
t1b select ...;
t1c commit;

txn2 3
t2a begin;
t2b select ...;
t2c commit;

permutation 6
t1a
t2a
t1b
t2b
t1c
t2c
