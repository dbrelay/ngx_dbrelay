/*
 * DB Relay is an HTTP module built on the NGiNX webserver platform which 
 * communicates with a variety of database servers and returns JSON formatted 
 * data.
 * 
 * Copyright (C) 2008-2010 Getco LLC
 * 
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) 
 * any later version. In addition, redistributions in source code and in binary 
 * form must 
 * include the above copyright notices, and each of the following disclaimers. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT OWNERS AND CONTRIBUTORS “AS IS” 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANY COPYRIGHT OWNERS OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "dbrelay.h"
#include "../include/dbrelay_config.h"

#define MAX_PATH_SZ 256

#if !HAVE_SEMUN
union semun {
   int              val;    /* Value for SETVAL */
   struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
   unsigned short  *array;  /* Array for GETALL, SETALL */
   struct seminfo  *__buf;  /* Buffer for IPC_INFO
                               (Linux specific) */
};
#endif

dbrelay_connection_t *buf;

key_t dbrelay_get_ipc_key()
{
   return ftok(DBRELAY_PREFIX, 1);
}
void dbrelay_create_shmem()
{
   key_t key;
   int shmid;
   int semid;
   dbrelay_connection_t *connections;
   union semun sem;
   void * buf;
   
   key = dbrelay_get_ipc_key();
   buf = malloc(DBRELAY_MAX_CONN * sizeof(dbrelay_connection_t));
   if (buf==NULL) perror("malloc");
   shmid = shmget(key, DBRELAY_MAX_CONN * sizeof(dbrelay_connection_t), IPC_CREAT | 0600);
   if (shmid==-1) perror("shmget");
   connections = (dbrelay_connection_t *) shmat(shmid, NULL, 0);
   memset(connections, '\0', DBRELAY_MAX_CONN * sizeof(dbrelay_connection_t));
   shmdt(connections);

   key = dbrelay_get_ipc_key();
   semid = semget(key, 1, IPC_CREAT | 0600);
   if (semid==-1) perror("semget");
   sem.val = 1;
   if (semctl(semid, 0, SETVAL, sem)==-1)
      perror("semctl");
}
dbrelay_connection_t *dbrelay_lock_shmem()
{
   key_t key;
   int semid;
   struct sembuf sb = {0, -1, SEM_UNDO};

   key = dbrelay_get_ipc_key();
   semid = semget(key, 1, 0);
   if (semid==-1) perror("semget");

   sb.sem_op = -1;
   if (semop(semid, &sb, 1)==1)
      perror("semop");

   return NULL;
}
void dbrelay_unlock_shmem()
{
   key_t key;
   int semid;
   struct sembuf sb = {0, -1, SEM_UNDO};

   key = dbrelay_get_ipc_key();
   semid = semget(key, 1, 0);

   if (semid==-1) perror("semget");
   sb.sem_op = 1;
   if (semop(semid, &sb, 1)==-1)
      perror("semop");
}
dbrelay_connection_t *dbrelay_get_shmem()
{
   key_t key;
   int shmid;
   dbrelay_connection_t *connections;

   dbrelay_lock_shmem();

   key = dbrelay_get_ipc_key();
   shmid = shmget(key, DBRELAY_MAX_CONN * sizeof(dbrelay_connection_t), 0600);
   if (shmid==-1) return NULL;

   connections = (dbrelay_connection_t *) shmat(shmid, NULL, 0);

   return connections;
}
void dbrelay_release_shmem(dbrelay_connection_t *connections)
{
   shmdt(connections);

   dbrelay_unlock_shmem();
}
void dbrelay_destroy_shmem()
{
   key_t key;
   int shmid;
   int semid;
   
   key = dbrelay_get_ipc_key();
   shmid = shmget(key, DBRELAY_MAX_CONN * sizeof(dbrelay_connection_t), IPC_CREAT | 0600);
   shmctl(shmid, IPC_RMID, NULL);

   key = dbrelay_get_ipc_key();
   semid = semget(key, 1, IPC_CREAT | 0600);
   semctl(semid, 0, IPC_RMID);
}
