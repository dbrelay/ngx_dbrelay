create table #t(
  t text
)
insert into #t (t) values ('Text Text Text')
select * from #t
drop table #t
