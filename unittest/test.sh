#!/bin/sh

make
if [ "$?" != "0" ]
then
	exit
fi
make install
~/nginx-0.7.11/sbin/nginx
sleep 1
kill `~/nginx-0.7.11/logs/nginx.pid`

sleep 3
python telnet.py
