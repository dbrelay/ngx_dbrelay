#!/bin/sh

if [ "$1" = "" ]
then
   echo "Usage: $0 <testname>"
   exit 1
fi

if [ "$1" = "-D" ]
then
   DEBUG=1
   shift
fi
if [ "$1" = "-C" ]
then
   OPTC="-c$2"
   shift
fi

OPTH="-h"`grep '^HOST=' PWD | cut -f2 -d=`
OPTD="-d"`grep '^DATABASE=' PWD | cut -f2 -d=`
if [ "$OPTD" = "-d" ]; then OPTD=""; fi
OPTU="-u"`grep '^USER=' PWD | cut -f2 -d=`
OPTW="-w"`grep '^PASSWORD=' PWD | cut -f2 -d=`
if [ "$OPTW" = "-w" ]; then OPTW=""; fi

TESTNAME=$1

#OPTC="-c${TESTNAME}"

JS=`which js 2> /dev/null`
if [ "$JS" = "" -o "x$DEBUG" = "x1" ]
then
   echo ../src/viaduct $OPTU $OPTH $OPTD $OPTC -tunittest -f${TESTNAME}.sql 2> /dev/null 
   ../src/viaduct $OPTU $OPTH $OPTD $OPTC -tunittest -f${TESTNAME}.sql 2> /dev/null 
else
   # spidermonkey's readline() doesn't differentiate between blank lines
   # and EOF, so the sed here adds a space to the begining of each line.
   ../src/viaduct $OPTU $OPTH $OPTD $OPTC -tunittest -f${TESTNAME}.sql 2> /dev/null | sed -e 's/^/ /' | $JS ${TESTNAME}.js
fi

