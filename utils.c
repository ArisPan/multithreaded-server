/* utils.c

   Sample code of 
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina 

   (c) S. Anastasiadis, G. Kappes 2016

*/
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"

/**
 * @name ERROR - Prints an error message and forces the rogram to exit.
 * @param msg: The message string.
 *
 * @return
 */ 
void ERROR(const char *msg) {
	fprintf(stderr, "Error in %s. Cause: ", msg);
	fflush(stdout);
	perror("");
	exit(EXIT_FAILURE);
}

/**
 * @name write_str_to_socker - Writes a message to the socket.
 * @param socket_fd: The socket descriptor.
 * @param buf: The buffer that contains the message.
 * @param numbytes: The length of the message.
 *
 * @return Number of bytes written.
 */
int write_str_to_socket(const int socket_fd, char *buf, const int numbytes) {
	char *ptr;
	int nwritten, nleft;
	int wsize;
	
  // write the amount of data to be sent.
	wsize = numbytes;
	if (write(socket_fd, &wsize, sizeof(wsize)) < 0)
		ERROR("write_to_socket()");
	
  // write data.
	ptr = buf;
	nleft = numbytes;
	while (nleft > 0 ) {
		if ((nwritten = write(socket_fd, ptr, nleft)) < 0)
			return 0;
		nleft -= nwritten;
		ptr += nwritten;
	}
	
	return numbytes;
}

/**
 * @name readstr_from_socker - Reads a message from the socket.
 * @param socket_fd: The socket descriptor.
 * @param buf: The buffer that will hold the message.
 * @param bufize: The size of the buffer.
 *
 * @return Number of bytes read.
 */
int read_str_from_socket(const int socket_fd, char *buf, const int bufsize)
{
	char *ptr;
	int nread, nleft, rc;
	int rsize;
	
  // read the amount of sent data. 
	if ((rc = read(socket_fd, &rsize, sizeof(rsize))) < 0) {
		// ERROR("read_from_socket()");
		printf("Error in read_str_from_socket(). fd = %d\n", socket_fd);
		exit(1);
	}
	else if (rc == 0)
		return 0;
	
  // read data.
	ptr = buf;
	nleft = rsize;
	while (nleft > 0 ) {
		if ((nread = read(socket_fd, ptr, nleft)) <= 0)
			return 0;

		if (nread > bufsize)
			return nread;
		
		nleft -= nread;
		ptr += nread;

	}  
	*ptr = '\0';
	return rsize;
}

