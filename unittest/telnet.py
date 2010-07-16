import sys
import telnetlib

HOST = "localhost"
PORT = "8080"
tn = telnetlib.Telnet(HOST, PORT)

#tn.write("GET /sql?sql_server=192.168.16.128&sql_database=getco&sql_user=sa&sql=select+%2A+from+dbrelay1%0Aselect+%2A+from+dbrelay2&query_tag=Example HTTP/1.1\n")
tn.write("POST /sql HTTP/1.1\n")
tn.write("Host: localhost\n")
tn.write("Content-length: 98\n")
tn.write("Connection: close\n")
tn.write("\n")
#tn.write("sql_server=dbserver&sql_database=dbname&sql_user=username&sql=exec+ExampleProc1&query_tag=Example")
tn.write("sql_server=192.168.16.128&sql_database=getco&sql_user=sa&sql=select * from dbrelay1 select * from dbrelay2&query_tag=Example")
tn.write("\n")

print tn.read_all()

