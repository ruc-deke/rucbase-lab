-- Lab3-查询执行  测试点3: 单表更新与条件查询
create table student (id int, name char(9), major char(32));
insert into student values (0, 'KangKanga', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
insert into student values (1, 'TomTomTom', 'Computer ScienceComputer Science');
insert into student values (2, 'JerryJerr', 'Computer ScienceComputer Science');
insert into student values (3, 'JackJackJ', 'Electrical Engineeringer Science');
insert into student values (3, 'JerryJerr', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
update student set major = 'Electrical Engineering' where id = 2;
select * from student where id>=1;
select * from student where id = 0;
update student set id = 100 where name = 'JerryJerry';
select * from student where id > 2;
select * from student where id < 101;
update student set name = '789123456' , major = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb' where id < 3;
select * from student where id = 2;
select * from student where name = '789123456';
