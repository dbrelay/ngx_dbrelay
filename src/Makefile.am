AM_CPPFLAGS     = -DCMDLINE @DB_INCS@
AM_LDFLAGS     = @DB_LIBS@ @DBRELAY_EXTRA_LIBS@
#bin_PROGRAMS = dbrelay 
sbin_PROGRAMS = connector
#dbrelay_SOURCES = dbrelay.h db.c log.c json.c stringbuf.c shmem.c client.c socket.c main.c admin.c status.c output/jsondict.c output/jsonarr.c output/csv.c libsybdb.a libtds.a 
connector_SOURCES = dbrelay.h db.c log.c json.c stringbuf.c client.c socket.c connector.c output/jsondict.c output/jsonarr.c output/csv.c
#EXTRA_dbrelay_SOURCES = mssql.h mssql.c vmysql.h mysql.c vodbc.h odbc.c
if FREETDS
#dbrelay_LDADD = mssql.o @DB_STATICLIBS@ @LIBS@
connector_LDADD = mssql.o @DB_STATICLIBS@
endif
if MYSQL
#dbrelay_LDADD = mysql.o @DB_STATICLIBS@ @LIBS@
connector_LDADD = mysql.o @DB_STATICLIBS@
endif
if ODBC
#dbrelay_LDADD = odbc.o
connector_LDADD = odbc.o
endif

