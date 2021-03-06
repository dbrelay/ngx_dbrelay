#
# DB Relay is an HTTP module built on the NGiNX webserver platform which 
# communicates with a variety of database servers and returns JSON formatted 
# data.
# 
# Copyright (C) 2008-2010 Getco LLC
# 
# This program is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, either version 3 of the License, or (at your option) 
# any later version. In addition, redistributions in source code and in binary 
# form must 
# include the above copyright notices, and each of the following disclaimers. 
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT OWNERS AND CONTRIBUTORS “AS IS” 
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
# ARE DISCLAIMED.  IN NO EVENT SHALL ANY COPYRIGHT OWNERS OR CONTRIBUTORS BE 
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
# POSSIBILITY OF SUCH DAMAGE.
#

AC_INIT(dbrelay_module, 0.01)
AC_CONFIG_SRCDIR(src/ngx_http_dbrelay_module.c)
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.0 $)

AC_PREFIX_DEFAULT(/usr/local)

# ------------------------------------------------------------
# Checks for programs.
# ------------------------------------------------------------
AC_PROG_CC

DBRELAY_BUILD=`git show --abbrev-commit | grep "^commit" | awk '{print $2}'`
AC_SUBST(DBRELAY_BUILD)

AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_HEADERS(include/config.h)
AC_CONFIG_FILES( Makefile \
        include/Makefile \
        src/Makefile \
)

VL_LIB_READLINE

AC_ARG_WITH(nginx,
AS_HELP_STRING([--with-nginx=DIR], [build with support for nginx in DIR]))
if test "$with_nginx"; then
        NGINX_INC="-I$with_nginx/include"
        CPPFLAGS="$CPPFLAGS $NGINX_INC"
fi

AC_ARG_WITH(freetds,
AS_HELP_STRING([--with-freetds=DIR], [build with support for FreeTDS in DIR]))
if test "$with_freetds"; then
        if echo "$with_freetds" | grep -v '^/'; then
                with_freetds="$PWD/$with_freetds"
        fi
	freetds=true
        DB_INCS="-I $with_freetds/include"
        #CPPFLAGS="$CPPFLAGS $DB_INCS"
        #DB_LIBS="-L $with_freetds/src/dblib/ -lsybdb -L $with_freetds/src/tds/.libs/ -ltds"
        DB_STATICLIBS="$with_freetds/src/dblib/.libs/libsybdb.a $with_freetds/src/tds/.libs/libtds.a"
        #LDFLAGS="$LDFLAGS $DB_LIBS"
        AC_DEFINE(HAVE_FREETDS, 1, [Define to 1 if building with FreeTDS support.])
        DB_MODULE="mssql.c"
fi
AM_CONDITIONAL(FREETDS, test "$freetds" = "true")

AC_ARG_WITH(mysql,
AS_HELP_STRING([--with-mysql=DIR], [build with support for MySQL in DIR]))
if test "$with_mysql"; then
        if echo "$with_mysql" | grep -v '^/'; then
                with_mysql="$PWD/$with_mysql"
        fi
	mysql=true
        DB_INCS="-I $with_mysql/include"
        #CPPFLAGS="$CPPFLAGS $DB_INCS"
        DB_LIBS="-lssl"
        DB_STATICLIBS="$with_mysql/lib/libmysqlclient.a"
        #LDFLAGS="$LDFLAGS $DB_LIBS"
        AC_DEFINE(HAVE_MYSQL, 1, [Define to 1 if building with MySQL support.])
        DB_MODULE="mysql.c"
fi

AC_SUBST(DB_LIBS)
AC_SUBST(DB_STATICLIBS)
AC_SUBST(DB_INCS)
AC_SUBST(DB_MODULE)

AM_CONDITIONAL(MYSQL, test "$mysql" = "true")

AC_ARG_WITH(odbc,
AS_HELP_STRING([--with-odbc=DIR], [build with support for ODBC in DIR]))
if test "$with_odbc"; then
        if echo "$with_odbc" | grep -v '^/'; then
                with_odbc="$PWD/$with_odbc"
        fi
        odbc=true
        AC_CHECK_LIB(iodbc, SQLConnect, [DB_LIBS="-L$with_odbc/lib -liodbc"],
                [AC_CHECK_LIB(odbc, SQLConnect, [DB_LIBS="-L$with_odbc/lib -lodbc"])] )
        DB_INCS="-I$with_odbc/include"
        AC_DEFINE(HAVE_ODBC, 1, [Define to 1 if building with ODBC support.])
        AC_SUBST(DB_LIBS)
        AC_SUBST(DB_INCS)
        DB_MODULE="odbc.c"
        AC_SUBST(DB_MODULE)

fi
AM_CONDITIONAL(ODBC, test "$odbc" = "true")

AC_ARG_WITH(magic,
AS_HELP_STRING([--with-magic=MAGIC], [prepend MAGIC to beginning of each query.]))
if test "$with_magic"; then
	DBRELAY_MAGIC=$with_magic
	AC_SUBST(DBRELAY_MAGIC)
fi

AC_CHECK_TYPES([semun])
if test "$ac_cv_type_semun" = "no"; then
  AC_MSG_CHECKING(if sem.h defines semun)
  AC_TRY_COMPILE([#include <sys/types.h>
      #include <sys/ipc.h>
      #include <sys/sem.h>],
  [union semun s;],
  [ac_cv_type_semun=yes AC_DEFINE(HAVE_SEMUN) AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)])
fi

DBRELAY_EXTRA_LIBS=
AC_CHECK_LIB(iconv, iconv_open, [DBRELAY_EXTRA_LIBS="$DBRELAY_EXTRA_LIBS -liconv"])
AC_CHECK_LIB(rt, clock_gettime, [DBRELAY_EXTRA_LIBS="$DBRELAY_EXTRA_LIBS -lrt"])
AC_SUBST(DBRELAY_EXTRA_LIBS)

AC_MSG_CHECKING([for SO_NOSIGPIPE])
AC_EGREP_CPP(we_have_sonosigpipe,
[#include <sys/socket.h>
#ifdef SO_NOSIGPIPE
we_have_sonosigpipe
#endif
], AC_DEFINE([HAVE_SO_NOSIGPIPE], [1], [Define to 1 if you have the SO_NOSIGPIPE socketoption.]) haveSO_NOSIGPIPE=yes, haveSO_NOSIGPIPE=no)
AC_MSG_RESULT([$haveSO_NOSIGPIPE])

AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_EGREP_CPP(we_have_MSG_NOSIGNAL,
[#include <linux/socket.h>
#ifdef MSG_NOSIGNAL
we_have_MSG_NOSIGNAL
#endif
], AC_DEFINE([HAVE_MSG_NOSIGNAL], [1], [Define to 1 if you have the MSG_NOSIGNAL socket flag.]) haveMSG_NOSIGNAL=yes, haveMSG_NOSIGNAL=no)
AC_MSG_RESULT([$haveMSG_NOSIGNAL])

AC_OUTPUT([src/config])
