AM_CPPFLAGS     = -DCMDLINE @DB_INCS@
AM_LDFLAGS     = @DB_LIBS@ @VIADUCT_EXTRA_LIBS@
bin_PROGRAMS = viaduct 
sbin_PROGRAMS = connector
viaduct_SOURCES = viaduct.h db.c log.c json.c stringbuf.c shmem.c client.c socket.c main.c admin.c libsybdb.a libtds.a
connector_SOURCES = viaduct.h db.c log.c json.c stringbuf.c shmem.c client.c socket.c connector.c 
EXTRA_viaduct_SOURCES = mssql.h mssql.c vmysql.h mysql.c
if FREETDS
viaduct_LDADD = mssql.o @DB_STATICLIBS@ @LIBS@
connector_LDADD = mssql.o @DB_STATICLIBS@
endif
if MYSQL
viaduct_LDADD = mysql.o @DB_STATICLIBS@ @LIBS@
connector_LDADD = mysql.o @DB_STATICLIBS@
endif

