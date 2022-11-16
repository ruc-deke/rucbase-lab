select * from student;
select * from grade;
select * from student where id>=2;
select * from student, grade;
select id, name, major, course from student, grade where student.id = grade.student_id;
#