/* client.c

	Assignment L1: Simple multi-threaded key-value server
	for the course MYY601 Operating Systems, University of Ioannina 

	Single thread implementation by S. Anastasiadis, G. Kappes 2016

	Multithreaded Implementation by Panagiotidis Aris, March 2020
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "utils.h"

#define SERVER_PORT     6767
#define BUF_SIZE        2048
#define MAXHOSTNAMELEN  1024
#define MAX_STATION_ID   128
#define ITER_COUNT         1
#define GET_MODE           1
#define PUT_MODE           2
#define USER_MODE          3
#define NUMBER_OF_THREADS  4
#define REQUESTS_PER_THREAD 1
#define MODE 1					// Single thread -> 0, Multi thread -> 1.

/**
 * @name print_usage - Prints usage information.
 * @return
 */
void print_usage() {
	fprintf(stderr, "Usage: client [OPTION]...\n\n");
	fprintf(stderr, "Available Options:\n");
	fprintf(stderr, "-h:             Print this help message.\n");
	fprintf(stderr, "-a <address>:   Specify the server address or hostname.\n");
	fprintf(stderr, "-o <operation>: Send a single operation to the server.\n");
	fprintf(stderr, "                <operation>:\n");
	fprintf(stderr, "                PUT:key:value\n");
	fprintf(stderr, "                GET:key\n");
	fprintf(stderr, "-i <count>:     Specify the number of iterations.\n");
	fprintf(stderr, "-g:             Repeatedly send GET operations.\n");
	fprintf(stderr, "-p:             Repeatedly send PUT operations.\n");
}

/**
 * @name talk - Sends a message to the server and prints the response.
 * @server_addr: The server address.
 * @buffer: A buffer that contains a message for the server.
 *
 * @return
 */
void talk(const struct sockaddr_in server_addr, char *buffer) {
	char rcv_buffer[BUF_SIZE];
	int socket_fd, numbytes;

	// create socket
	if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR("socket()");
	}

	// connect to the server.
	if (connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
		ERROR("connect()");
	}

	// send message.
	write_str_to_socket(socket_fd, buffer, strlen(buffer));

	// receive results.
	printf("Result: ");
	do {
		memset(rcv_buffer, 0, BUF_SIZE);
		numbytes = read_str_from_socket(socket_fd, rcv_buffer, BUF_SIZE);
		if (numbytes != 0)
    		printf("%s", rcv_buffer); // print to stdout
	} while (numbytes > 0);
	printf("\n");

	// close the connection to the server.
	close(socket_fd);
}

void *func(void *arg) {

	struct sockaddr_in server_addr = *(const struct sockaddr_in*) arg;
	int random_station, value, request_type, i;
	char snd_buffer[BUF_SIZE];

	for (i = 0; i < REQUESTS_PER_THREAD; i++) {

		request_type = rand() % 2;

		if (request_type) {

			// GET Request.
			memset(snd_buffer, 0, BUF_SIZE);
			
			random_station = rand() % (MAX_STATION_ID + 1);
			
			sprintf(snd_buffer, "GET:station.%d", random_station);
	    	printf("Operation: %s\n", snd_buffer);
			talk(server_addr, snd_buffer);
		}
		else {

			// PUT Request.
			memset(snd_buffer, 0, BUF_SIZE);
			
			value = rand() % 65 + (-20);
	    	random_station = rand() % (MAX_STATION_ID + 1);
			
			sprintf(snd_buffer, "PUT:station.%d:%d", random_station, value);
	    	printf("Operation: %s\n", snd_buffer);
			talk(server_addr, snd_buffer);
		}
	}

	return EXIT_SUCCESS;
}

/**
 * @name main - The main routine.
 */
int main(int argc, char **argv) {

	char *host = NULL;
	char *request = NULL;
	int mode = 0;
	int option = 0;
	int count = ITER_COUNT;
	char snd_buffer[BUF_SIZE];
	int station, value;
	struct sockaddr_in server_addr;
	struct hostent *host_info;

	pthread_t thread_id[NUMBER_OF_THREADS];
	int i, thread_check;
	
	struct timespec start, finish;
	long seconds, nanoseconds;
	long total_time;

	clock_t start_sec, end_sec;
	double cpu_time_used;
	start_sec = clock();

	if (MODE == 0) {

		// Parse user parameters.
		while ((option = getopt(argc, argv,"i:hgpo:a:")) != -1) {
			switch (option) {
				case 'h':
					print_usage();
					exit(0);
				case 'a':
					host = optarg;
					printf("Host: %s\n", host);
					break;
				case 'i':
					count = atoi(optarg);
					break;
				case 'g':
					if (mode) {
						fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
						exit(EXIT_FAILURE);
					}
					mode = GET_MODE;
					break;
				case 'p': 
					if (mode) {
						fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
						exit(EXIT_FAILURE);
					}
					mode = PUT_MODE;
					break;
				case 'o':
					if (mode) {
						fprintf(stderr, "You can only specify one of the following: -r, -w, -o\n");
						exit(EXIT_FAILURE);
					}
					mode = USER_MODE;
					request = optarg;
					break;
				default:
					print_usage();
					exit(EXIT_FAILURE);
			}
		}

		// Check parameters.
		if (!mode) {
			fprintf(stderr, "Error: One of -g, -p, -o is required.\n\n");
			print_usage();
			exit(0);
		}
		if (!host) {
			fprintf(stderr, "Error: -a <address> is required.\n\n");
			print_usage();
			exit(0);
		}

		// get the host (server) info
		if ((host_info = gethostbyname(host)) == NULL) { 
			ERROR("gethostbyname()"); 
		}

		// create socket adress of server (type, IP-adress and port number)
		bzero(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr = *((struct in_addr*)host_info->h_addr);
		server_addr.sin_port = htons(SERVER_PORT);

		if (mode == USER_MODE) {
			memset(snd_buffer, 0, BUF_SIZE);
			strncpy(snd_buffer, request, strlen(request));
			printf("Operation: %s\n", snd_buffer);
			talk(server_addr, snd_buffer);
		} else {
			while(--count>=0) {
				for (station = 0; station <= MAX_STATION_ID; station++) {
					memset(snd_buffer, 0, BUF_SIZE);
					if (mode == GET_MODE) {
						// Repeatedly GET.
						sprintf(snd_buffer, "GET:station.%d", station);
					} else if (mode == PUT_MODE) {
						// Repeatedly PUT.
						// create a random value.
						value = rand() % 65 + (-20);
						sprintf(snd_buffer, "PUT:station.%d:%d", station, value);
					}
					printf("Operation: %s\n", snd_buffer);
					talk(server_addr, snd_buffer);
				}
			}
		}
	}
	else {

		// get the host (server) info
		if ((host_info = gethostbyname("localhost")) == NULL) { 
			ERROR("gethostbyname()"); 
		}

		// create socket adress of server (type, IP-adress and port number)
		bzero(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr = *((struct in_addr*)host_info->h_addr);
		server_addr.sin_port = htons(SERVER_PORT);

		if (clock_gettime(CLOCK_REALTIME, &start) == -1) {

			perror("clock_gettime()");
			exit(1);
		}

		for (i = 0; i < NUMBER_OF_THREADS; i++) {

			thread_check = pthread_create(&thread_id[i], NULL, func, (void *) &server_addr);
			if (thread_check != 0) {

				perror("pthread_create()");
				exit(1);
			}
		}

		for (i = 0; i < NUMBER_OF_THREADS; i++) {

			pthread_join(thread_id[i], NULL);
		}

		if (clock_gettime(CLOCK_REALTIME, &finish) == -1) {

			perror("clock_gettime()");
			exit(1);
		}

		seconds = finish.tv_sec - start.tv_sec;
		nanoseconds = finish.tv_nsec - start.tv_nsec;

		total_time = ((long)seconds * (long)1000000000) + (long)nanoseconds;
		printf("Total time (nanosec): %.0ld\n", total_time);

		end_sec = clock();
		cpu_time_used = ((double) (end_sec - start_sec)) / CLOCKS_PER_SEC;
		fprintf(stderr, "Total time (seconds) %.4f\n", cpu_time_used);
	}

	return 0;
}