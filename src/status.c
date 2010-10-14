#include "dbrelay.h"
#include "../include/dbrelay_config.h"

u_char *dbrelay_db_status(dbrelay_request_t *request)
{
   dbrelay_connection_t *connections;
   dbrelay_connection_t *conn;
   json_t *json = json_new();
   int i;
   char tmpstr[100];
   u_char *json_output;
   struct tm *ts;


   json_new_object(json);
   json_add_key(json, "status");
   json_new_object(json);

   json_add_key(json, "info");
   json_new_object(json);
   json_add_string(json, "build", DBRELAY_BUILD);
   sprintf(tmpstr, "0x%08x", dbrelay_get_ipc_key());
   json_add_string(json, "ipckey", tmpstr);
   json_end_object(json);

   json_add_key(json, "connections");
   json_new_array(json);

   connections = dbrelay_time_get_shmem(request);

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     conn = &connections[i];
     if (connections[i].pid!=0) {
        json_new_object(json);
        sprintf(tmpstr, "%u", conn->slot);
        json_add_number(json, "slot", tmpstr);
        sprintf(tmpstr, "%u", conn->pid);
        json_add_number(json, "pid", tmpstr);
        json_add_string(json, "name", conn->connection_name ? conn->connection_name : "");
        ts = localtime(&conn->tm_create);
        strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M:%S", ts);
        json_add_string(json, "tm_created", tmpstr);
        ts = localtime(&conn->tm_accessed);
        strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M:%S", ts);
        json_add_string(json, "tm_accessed", tmpstr);
        json_add_string(json, "sql_server", conn->sql_server ? conn->sql_server : "");
        json_add_string(json, "sql_port", conn->sql_port ? conn->sql_port : "");
        json_add_string(json, "sql_database", conn->sql_database ? conn->sql_database : "");
        json_add_string(json, "sql_user", conn->sql_user ? conn->sql_user : "");
        sprintf(tmpstr, "%ld", conn->connection_timeout);
        json_add_number(json, "connection_timeout", tmpstr);
        sprintf(tmpstr, "%u", conn->in_use);
        json_add_number(json, "in_use", tmpstr);
        json_add_string(json, "sock_path", conn->sock_path);
        sprintf(tmpstr, "%u", conn->helper_pid);
        json_add_number(json, "helper_pid", tmpstr);
        json_end_object(json);
     }
   }

   dbrelay_time_release_shmem(request, connections);

   json_end_array(json);
   json_end_object(json);
   json_end_object(json);

   json_output = (u_char *) json_to_string(json);
   json_free(json);

   return json_output;
}
