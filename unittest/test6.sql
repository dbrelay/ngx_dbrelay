create table #t(
  first varchar(254),
  second varchar(32)
)
insert into #t (first, second)
values ('12345678901234567890123456789012', '-')
select * from #t
drop table #t
