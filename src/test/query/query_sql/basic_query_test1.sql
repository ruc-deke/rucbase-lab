-- Lab3-查询执行  测试点1: 尝试建表
create table student (id int, name char(32), major char(32));
create table grade (course char(32), student_id int, score float);
show tables;
drop table student;
show tables;
create table grade (id int);
drop table t;
show tables;
create index grade(id);
create index grade(student_id);
create index student(id);

drop table grade;
