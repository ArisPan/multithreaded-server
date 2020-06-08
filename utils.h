/* utils.h

   Sample code of 
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina 

   (c) S. Anastasiadis, G. Kappes 2016

*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <assert.h>
#include <pthread.h>
#include <netdb.h>

void ERROR(const char *msg);

// write to socket 'numbytes' bytes from buffer 'buf'
int write_str_to_socket(const int socket_fd, char *buf, const int numbytes);

// read from socket into buffer 'buf' a stream of bytes that were 
// sent using write_to_socket(); terminate data with '\0'.
int read_str_from_socket(const int socket_fd, char *buf, const int bufsize);

