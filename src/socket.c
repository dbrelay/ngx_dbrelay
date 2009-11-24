#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAIN 0

#if !MAIN
#include "dbrelay.h"
#include "../include/dbrelay_config.h"
#else
#define DBRELAY_SOCKET_BUFSIZE 20
#define HAVE_SO_NOSIGPIPE 1
#define NET_FLAGS 0
#endif

#define DEBUG 0

#define SEL_WRITE 0x01
#define SEL_READ  0x02
#define SEL_ERROR 0x04

int dbrelay_socket_wait(int s, int mode, int timeout);

unsigned int
dbrelay_socket_create(char *sock_path)
{
   unsigned int s;
   int ret, len;
   struct sockaddr_un local;
   unsigned int nonblocking = 1;

   s = socket(AF_UNIX, SOCK_STREAM, 0);

   if (ioctl(s, FIONBIO, &nonblocking) < 0) {
      if (DEBUG) fprintf(stderr, "Could not use non-blocking socket\n"); 
      return -1;
   }

   local.sun_family = AF_UNIX;  
   strcpy(local.sun_path, sock_path);
   unlink(local.sun_path);
   len = strlen(local.sun_path) + sizeof(local.sun_family) + 1;
   ret = bind(s, (struct sockaddr *)&local, len);

   listen(s, 30);

   return s;
}

unsigned int
dbrelay_socket_accept(unsigned int s)
{
   unsigned int s2;
   struct sockaddr_un remote;
   int ret;
#if HAVE_SO_NOSIGPIPE
   int on = 1;
#endif

   socklen_t len = sizeof(struct sockaddr_un);

   ret = dbrelay_socket_wait(s, SEL_READ, 30);
   if (ret<=0) return ret;

   s2 = accept(s, (struct sockaddr *) &remote, &len);

#if HAVE_SO_NOSIGPIPE
      setsockopt(s2, SOL_SOCKET, SO_NOSIGPIPE, (void *)&on, sizeof(on));
#endif
   return s2;
}

int
dbrelay_socket_connect(char *sock_path)
{
   int s, len, ret;
   struct sockaddr_un remote;
   unsigned int nonblocking = 1;

#if HAVE_SO_NOSIGPIPE
   int on = 1;
#endif


   if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
       return -1;
   }

   if (ioctl(s, FIONBIO, &nonblocking) < 0) {
      if (DEBUG) fprintf(stderr, "Could not use non-blocking socket\n"); 
      return -1;
   }

   if (DEBUG) printf("Trying to connect (%s)...\n", sock_path);

   remote.sun_family = AF_UNIX;
   strcpy(remote.sun_path, sock_path);
   len = strlen(remote.sun_path) + sizeof(remote.sun_family) + 1;

   if ((ret = connect(s, (struct sockaddr *)&remote, len)) == -1) {
      if (ret == EINPROGRESS) {
         ret = dbrelay_socket_wait(s, SEL_WRITE | SEL_ERROR, 60);
         if (ret==-1) {
            if (DEBUG) fprintf(stderr, "connect timeout\n"); 
            return -1;
         }
      } else {
         if (DEBUG) perror("connect"); 
         return -1;
      }
   }
   if (DEBUG) printf("Connected.\n");

#if HAVE_SO_NOSIGPIPE
   setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void *)&on, sizeof(on));
#endif

   return s;
}
int
dbrelay_socket_send_string(int s, char *str)
{
   ssize_t bytes_sent;
   ssize_t bytes_to_send = strlen(str);
   int pos = 0;

   while (bytes_to_send > 0) {
        if (dbrelay_socket_wait(s, SEL_WRITE, 0)==-1) {
           if (DEBUG) fprintf(stderr, "error waiting to send\n");
           return -1;
        }
   	bytes_sent = send(s, &str[pos], bytes_to_send, NET_FLAGS);
	if (bytes_sent == -1) {
           if (DEBUG) perror("send");
           return -1;
        }
	bytes_to_send -= bytes_sent;
	pos += bytes_sent;
   }
   return 0;
}
int
dbrelay_socket_recv_string(int s, char *in_buf, int *in_ptr, char *out_buf, int timeout)
{
   int t, len = 0;
   int i, ret;
   int have_space, have_available;
   int done = 0;
   int out_ptr = 0;

   do {
      if (DEBUG) printf("ptr %d\n", *in_ptr);
      if (*in_ptr >= DBRELAY_SOCKET_BUFSIZE - 1) *in_ptr=-1;
      if (*in_ptr==-1) {
         if ((ret=dbrelay_socket_wait(s, SEL_READ, timeout))==-1) {
            if (DEBUG) fprintf(stderr, "wait for socket read failed\n"); 
            return ret;
         }
         if ((t=recv(s, in_buf, DBRELAY_SOCKET_BUFSIZE - 1, NET_FLAGS))<=0) {
	   if (t < 0) {
             if (errno==EINTR) {
                 printf("got EINTR\n"); 
                 continue;
             }
             if (DEBUG) perror("recv"); 
           } else {
             if (DEBUG) printf("Server closed connection\n");
           }
           if (DEBUG) printf("Returning %d\n", t);
           return t;
         }
         if (DEBUG) printf("got %d bytes\n", t);
         in_buf[t] = '\0';
         *in_ptr=0;
         if (DEBUG) printf("buf = %s!\n", in_buf);
      } // else (*in_ptr)++;
      len = strlen(in_buf);

      for (i=*in_ptr;in_buf[i]!='\n' && i<len; i++);
      have_available = i - *in_ptr;
      have_space = DBRELAY_SOCKET_BUFSIZE - out_ptr - 1;

      if (DEBUG) printf("have avail = %d\n", have_available);
      if (DEBUG) printf("have space = %d\n", have_space);
      if (DEBUG) printf("len = %d\n", len);

      if (have_space > have_available) {
         strncpy(&out_buf[out_ptr], &in_buf[*in_ptr], have_available);
         out_buf[out_ptr + have_available]='\0';
         out_ptr += have_available;

         *in_ptr += have_available;
         if (*in_ptr<len && in_buf[*in_ptr]=='\n') {
            (*in_ptr)++;
            if (*in_ptr >= len) *in_ptr=-1;
            return 1;
         }
         if (*in_ptr >= len) *in_ptr=-1;
         if (DEBUG) printf("in_ptr now %d\n", *in_ptr);
      } else {
         strncpy(&out_buf[out_ptr], &in_buf[*in_ptr], have_space);
         out_buf[DBRELAY_SOCKET_BUFSIZE-1]='\0';
         *in_ptr += have_space;
         if (DEBUG) printf("in_ptr = %d\n", *in_ptr);
         if (*in_ptr >= DBRELAY_SOCKET_BUFSIZE - 1) *in_ptr=-1;
         return 1;
      }
   } while (!done);

   if (DEBUG) printf("returning %s\n", out_buf);
   return 1; 
}
#if 0
int
dbrelay_socket_recv_string(int s, char *in_buf, int *in_ptr, char *out_buf)
{
   int t;
   int i;
   int len;
   int done = 0;
   int out_ptr = 0;
   int end;

   do {
      if (DEBUG) printf("ptr %d\n", *in_ptr);
      if (*in_ptr==-1) {
         if (dbrelay_socket_wait(s, SEL_READ, 0)==-1) {
            return -1;
         }
         if ((t=recv(s, in_buf, DBRELAY_SOCKET_BUFSIZE - 1, NET_FLAGS))<=0) {
	   if (t < 0) {
             if (DEBUG) perror("recv"); 
           } else {
             if (DEBUG) printf("Server closed connection\n");
           }
           return t;
         }
         if (DEBUG) printf("got %d bytes\n", t);
         in_buf[t] = '\0';
         *in_ptr=0;
         if (DEBUG) printf("buf = %s!\n", in_buf);
      } else (*in_ptr)++;
      len = strlen(in_buf);
      if (DEBUG) printf("ptr %d len %d\n", *in_ptr, len);
      for (i=*in_ptr;in_buf[i]!='\n' && i<len; i++);
      if (DEBUG) printf("i = %d\n", i);
      end = (i - *in_ptr);

      if (DEBUG) printf("out_ptr = %d\n", out_ptr);
      if (DEBUG) printf("end1 = %d\n", end);
      if (out_ptr + end > DBRELAY_SOCKET_BUFSIZE - 1) {
         end =  DBRELAY_SOCKET_BUFSIZE - out_ptr - 1;
         strncpy(&out_buf[out_ptr], &in_buf[*in_ptr], end);
         *in_ptr--;
         done = 1;
      } else {
         strncpy(&out_buf[out_ptr], &in_buf[*in_ptr], end);
      }
      if (DEBUG) printf("end2 = %d\n", end);
      out_buf[out_ptr + end]='\0';
      if (DEBUG) printf("out_buf = %s\n", out_buf);
      if (i>len-1) {
          out_ptr += end;
          if (DEBUG) printf("out_ptr = %d\n", out_ptr);
          //out_buf += end;
          // exhausted the buffer, fetch another
          *in_ptr=-1;
          if (DEBUG) printf("in_ptr = -1");
      } else {
          *in_ptr += end; 
          // found a newline, we are done for now
          done = 1;
      }
      if (DEBUG) printf("echo> %s\n", out_buf);
      if (*in_ptr>=DBRELAY_SOCKET_BUFSIZE) exit(1);
   } while (!done);

   if (DEBUG) printf("returning %s\n", out_buf);
   return 1; 
}
#endif
#if MAIN
int main(int argc, char **argv)
{
unsigned int s, s2;
//char out_buf[DBRELAY_SOCKET_BUFSIZE];
char out_buf[300];
char in_buf[DBRELAY_SOCKET_BUFSIZE];
int in_ptr = -1;
int ret;

    if (strcmp(argv[1], "client")) {
       s = dbrelay_socket_create("/tmp/socket1");
       if (dbrelay_socket_wait(s, SEL_READ, 0)==-1) {
          return -1;
       }
       s2 = dbrelay_socket_accept(s);
       printf("socket: %d\n", s2);
       while (ret = dbrelay_socket_recv_string(s2, in_buf, &in_ptr, out_buf)) {
         printf("got: %s\n", out_buf);
       }
       printf("socket: %d\n", ret);
       //printf("socket: %d\n", s2);
       //perror("accept");
    } else  {
       s = dbrelay_socket_connect("/tmp/socket1");
       dbrelay_socket_send_string(s, "123456789012345678901234567890\n1234567890\n");
       close(s);
    }
}
#endif

int
dbrelay_socket_wait(int s, int mode, int timeout)
{
   struct timeval tv;
   fd_set fds[3];
   fd_set *readfds = NULL, *writefds = NULL, *errorfds = NULL;
   int ret;

   if (mode & SEL_READ) {
      readfds = &fds[0];
      FD_ZERO(readfds);
      FD_SET(s, readfds);
   }
   if (mode & SEL_WRITE) {
      writefds = &fds[1];
      FD_ZERO(writefds);
      FD_SET(s, writefds);
   }
   if (mode & SEL_ERROR) {
      errorfds = &fds[2];
      FD_ZERO(errorfds);
      FD_SET(s, errorfds);
   }

   tv.tv_sec = timeout; // wait timeout seconds.
   tv.tv_usec = 0;

   ret = select(s + 1, readfds, writefds, errorfds, (timeout) ? &tv : NULL);

/*
   if (mode & SEL_READ) do {
      ret = select(s + 1, readfds, writefds, errorfds, (timeout) ? &tv : NULL);
      if (ret==-1) return ret;
   } while (!FD_ISSET(s, readfds));

   if (mode & SEL_WRITE) do {
      ret = select(s + 1, readfds, writefds, errorfds, (timeout) ? &tv : NULL);
      if (ret==-1) return ret;
   } while (!FD_ISSET(s, writefds));
*/

   return ret;
}
