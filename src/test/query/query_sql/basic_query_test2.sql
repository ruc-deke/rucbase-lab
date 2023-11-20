-- Lab3-查询执行  测试点2: 单表插入与条件查询
create table student (id int, name char(9), major char(32));
insert into student values (0, 'KangKanga', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
insert into student values (1, 'TomTomTom', 'Computer ScienceComputer Science');
insert into student values (2, 'JerryJerr', 'Computer ScienceComputer Science');
insert into student values (3, 'JackJackJ', 'Electrical Engineeringer Science');
insert into student values (3, 'JerryJerr', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
select * from student where id >= 1;
select id from student where id = 2;
select major from student where name = 'JerryJerr';
select name,major from student where name = 'JerryJerr' and id = 0;
select name,major from student where name = 'JerryJerr' and id = 2;
select name,id from student where major = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
select myname from student;
select name from student where myname = 'aaa';
