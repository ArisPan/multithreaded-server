/* server.c

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
#include <signal.h>
#include <sys/stat.h>
#include "utils.h"
#include "kissdb.h"

#define MY_PORT                 6767
#define BUF_SIZE                1160
#define KEY_SIZE                 128
#define HASH_SIZE               1024
#define VALUE_SIZE              1024
#define MAX_PENDING_CONNECTIONS   5
#define NUMBER_OF_CONSUMER_THREADS 8
#define QUEUE_SIZE 100
#define BILLION 1000000000

pthread_mutex_t full_queue_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_queue_cond_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t empty_queue_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty_queue_cond_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t writer_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t writer_cond_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t reader_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t reader_cond_var = PTHREAD_COND_INITIALIZER;

pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t enqueue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t update_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// Definition of the operation type.
typedef enum operation {
	PUT,
	GET
} Operation; 

// Definition of the request.
typedef struct request {
	Operation operation;
	char key[KEY_SIZE];  
	char value[VALUE_SIZE];
} Request;

// Definition of the request new_request to be stored in queue.
typedef struct connection_info {

	int fd;		// File descriptor returned by accept() in producer (main) thread.
	struct timespec connection_start;
} connection_info;

// Definition of the database.
KISSDB *db = NULL;

pthread_t thread_id[NUMBER_OF_CONSUMER_THREADS];

double total_waiting_time = 0;	// ...of requests in queue.
double total_service_time = 0;	// ...of completed requests.
int completed_requests = 0;

int reader_count = 0;		// Threads serving a GET request. Unlimited at a time.
int writer_count = 0;		// Threads serving a PUT request. Max 1 at a time.

int stop = 0;		// Used for informing consumer threads to wrap it up.

// Definition of the queue holding the request new_requests.
connection_info queue[QUEUE_SIZE];
int queue_is_empty = 1;
int queue_is_full = 0;
int head = 0;
int tail = 0;
int items_in_queue = 0;

void print_globals(char *called_by) {

	printf("(Coming from %s)\n"
		"queue_is_empty = %d\n"
		"queue_is_full = %d\n"
		"head = %d\n"
		"tail = %d\n"
		"items_in_queue = %d\n\n",
		called_by, queue_is_empty, queue_is_full, head, tail, items_in_queue);
}

void enqueue(connection_info new_connection_info) {

	if (!(tail % QUEUE_SIZE)) { tail = 0; }

	queue[tail] = new_connection_info;
	tail++;
	items_in_queue++;
}

connection_info dequeue() {

	if (!(head % QUEUE_SIZE)) { head = 0; }

	head++;
	items_in_queue--;

	return queue[head - 1];
}

int check_if_queue_is_empty() {

	queue_is_empty = (items_in_queue == 0);
	return queue_is_empty;
}

int check_if_queue_is_full() {

	queue_is_full = (items_in_queue == QUEUE_SIZE);
	return queue_is_full;
}

void signal_handler(int sigid) {

	stop = 1;

	fprintf(stderr, "Completed requests: %d\n", completed_requests);
	fprintf(stderr, "Total waiting time (nanosec): %.0lf\n", total_waiting_time);
	fprintf(stderr, "Average waiting time (nanosec): %.0lf\n", total_waiting_time / (double) completed_requests);
	fprintf(stderr, "Total service time (nanosec): %.0lf\n", total_service_time);
	fprintf(stderr, "Average service time (nanosec): %.0lf\n", total_service_time / (double) completed_requests);

	// Destroy the database.
	// Close the database.
	KISSDB_close(db);

	// Free memory.
	if (db)
		free(db);
	db = NULL;

	_exit(1);
}

/**
 * @name parse_request - Parses a received message and generates a new request.
 * @param buffer: A pointer to the received message.
 *
 * @return Initialized request on Success. NULL on Error.
 */
Request *parse_request(char *buffer) {
	char *token = NULL;
	Request *req = NULL;

	// Check arguments.
	if (!buffer)
		return NULL;

	// Prepare the request.
	req = (Request *) malloc(sizeof(Request));
	memset(req->key, 0, KEY_SIZE);
	memset(req->value, 0, VALUE_SIZE);

	// Extract the operation type.
	token = strtok(buffer, ":");    
	if (!strcmp(token, "PUT")) {
		req->operation = PUT;
	} else if (!strcmp(token, "GET")) {
		req->operation = GET;
	} else {
		free(req);
		return NULL;
	}

	// Extract the key.
	token = strtok(NULL, ":");
	if (token) {
		strncpy(req->key, token, KEY_SIZE);
	} else {
		free(req);
		return NULL;
	}

	// Extract the value.
	token = strtok(NULL, ":");
	if (token) {
		strncpy(req->value, token, VALUE_SIZE);
	} else if (req->operation == PUT) {
		free(req);
		return NULL;
	}
	return req;
}

/*
 * @name process_request - Process a client request.
 * 
 * @return
 */
void process_request(void *arg) {
	
	connection_info new_request;

	char response_str[BUF_SIZE], request_str[BUF_SIZE];
	int numbytes = 0;
	Request *request = NULL;

	struct timespec start, finish;
	long seconds_in_queue, nanoseconds_in_queue;
	long seconds_of_service, nanoseconds_of_service;
	double time_spent_in_queue;		// In nanoseconds.
	double service_time;			// In nanoseconds.

	sigset_t set;

	sigaddset(&set, SIGTSTP);

	pthread_sigmask(SIG_BLOCK, &set, NULL);

	while (!stop) {

		pthread_mutex_lock(&empty_queue_cond_mutex);
		while (check_if_queue_is_empty()) {
			// fprintf(stderr, "Queue is empty, waiting...\n");
			pthread_cond_wait(&empty_queue_cond_var, &empty_queue_cond_mutex);
		}
		pthread_mutex_unlock(&empty_queue_cond_mutex);

	    // Clean buffers.
		memset(response_str, 0, BUF_SIZE);
		memset(request_str, 0, BUF_SIZE);
		
		pthread_mutex_lock(&dequeue_mutex);
		if (check_if_queue_is_empty()) {
			pthread_mutex_unlock(&dequeue_mutex);
			continue;
		}

		new_request = dequeue();

		// get time before serving the request.
		if (clock_gettime(CLOCK_REALTIME, &start) == -1) {

			perror("clock_gettime()");
			exit(1);
		}

		seconds_in_queue = start.tv_sec - new_request.connection_start.tv_sec;
		nanoseconds_in_queue = start.tv_nsec - new_request.connection_start.tv_nsec;

		time_spent_in_queue = ((double)seconds_in_queue * (double)BILLION) + ((double)nanoseconds_in_queue);

		total_waiting_time += time_spent_in_queue;

		pthread_mutex_unlock(&dequeue_mutex);

		pthread_mutex_lock(&full_queue_cond_mutex);
		if (!check_if_queue_is_full()) {

			pthread_cond_signal(&full_queue_cond_var);
		}
		pthread_mutex_unlock(&full_queue_cond_mutex);

		// receive message.
		numbytes = read_str_from_socket(new_request.fd, request_str, BUF_SIZE);

	    // parse the request.
		if (numbytes) {
			request = parse_request(request_str);
			if (request) {
				pthread_mutex_lock(&writer_cond_mutex);
				while (writer_count == 1) {

					pthread_cond_wait(&writer_cond_var, &writer_cond_mutex);
				}
				pthread_mutex_unlock(&writer_cond_mutex);
				switch (request->operation) {
					case GET:

					pthread_mutex_lock(&reader_mutex);
					reader_count++;
					pthread_mutex_unlock(&reader_mutex);

	            	// Read the given key from the database.
					if (KISSDB_get(db, request->key, request->value))
						sprintf(response_str, "GET ERROR\n");
					else
						sprintf(response_str, "GET OK: %s\n", request->value);

					pthread_mutex_lock(&reader_cond_mutex);
					reader_count--;

					if (reader_count == 0) {
						pthread_cond_signal(&reader_cond_var);
					}
					pthread_mutex_unlock(&reader_cond_mutex);
					break;

					case PUT:
					pthread_mutex_lock(&reader_cond_mutex);
					while (reader_count != 0) {

						pthread_cond_wait(&reader_cond_var, &reader_cond_mutex);
					}
					pthread_mutex_unlock(&reader_cond_mutex);

					pthread_mutex_lock(&writer_mutex);
					writer_count = 1;

	            	// Write the given key/value pair to the database.
					if (KISSDB_put(db, request->key, request->value)) 
						sprintf(response_str, "PUT ERROR\n");
					else
						sprintf(response_str, "PUT OK\n");

					writer_count = 0;

					pthread_mutex_lock(&writer_cond_mutex);
					if (writer_count == 0) {

						pthread_cond_broadcast(&writer_cond_var);
					}
					pthread_mutex_unlock(&writer_cond_mutex);
					pthread_mutex_unlock(&writer_mutex);
					break;
					default:
	            	// Unsupported operation.
					sprintf(response_str, "UNKOWN OPERATION\n");
				}

				// get time after serving the request.
				if (clock_gettime(CLOCK_REALTIME, &finish) == -1) {

					perror("clock_gettime()");
					exit(1);
				}
				seconds_of_service = finish.tv_sec - new_request.connection_start.tv_sec;
				nanoseconds_of_service = finish.tv_nsec - new_request.connection_start.tv_nsec;

				service_time = ((double)seconds_of_service * (double)BILLION) + ((double)nanoseconds_of_service);

				// Reply to the client.
				write_str_to_socket(new_request.fd, response_str, strlen(response_str));

				pthread_mutex_lock(&update_stats_mutex);
				total_service_time += service_time;
				completed_requests++;
				pthread_mutex_unlock(&update_stats_mutex);

				if (request)
					free(request);
				request = NULL;
			}
			else {
				// Send an Error reply to the client.
				sprintf(response_str, "FORMAT ERROR\n");
				write_str_to_socket(new_request.fd, response_str, strlen(response_str));
			}
		}
		else {
			// Send an Error reply to the client.
			sprintf(response_str, "FORMAT ERROR\n");
			write_str_to_socket(new_request.fd, response_str, strlen(response_str));
		}
	    
		close(new_request.fd);
	}

	return;
}

/*
 * @name main - The main routine.
 *
 * @return 0 on success, 1 on error.
 */
int main() {

	int socket_fd;		// listen on this socket for new connections
	socklen_t clen;
	struct sockaddr_in server_addr,	// my address information
						client_addr;	// connector's address information
	connection_info new_request;
	int thread_check, i;

	struct sigaction sact;
	sact.sa_handler = signal_handler;	// Handler for SIGTSTP.
	sigemptyset(&sact.sa_mask);			// No other signal to block.
	sact.sa_flags = 0;

	if (sigaction(SIGTSTP, &sact, NULL) < 0) {

		perror("Failed to set action for SIGTSTP");
	}

	for (i = 0; i < NUMBER_OF_CONSUMER_THREADS; i++) {

		thread_check = pthread_create(&thread_id[i], NULL, (void *) process_request, NULL);
		if (thread_check != 0) {

			perror("pthread_create()");
			exit(1);
		}
	}

	// create socket
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		ERROR("socket()");

	// Ignore the SIGPIPE signal in order to not crash when a
	// client closes the connection unexpectedly.
	signal(SIGPIPE, SIG_IGN);

	// create socket adress of server (type, IP-adress and port number)
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // any local interface
	server_addr.sin_port = htons(MY_PORT);
  
	// bind socket to address
	if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
		ERROR("bind()");

	// start listening to socket for incomming connections
	listen(socket_fd, MAX_PENDING_CONNECTIONS);
	fprintf(stderr, "(Info) main: Listening for new connections on port %d ...\n", MY_PORT);
	clen = sizeof(client_addr);

	// Allocate memory for the database.
	if (!(db = (KISSDB *)malloc(sizeof(KISSDB)))) {
		fprintf(stderr, "(Error) main: Cannot allocate memory for the database.\n");
		return 1;
	}

	// Open the database.
	if (KISSDB_open(db, "mydb.db", KISSDB_OPEN_MODE_RWCREAT, HASH_SIZE, KEY_SIZE, VALUE_SIZE)) {
		fprintf(stderr, "(Error) main: Cannot open the database.\n");
		return 1;
	}

	// main loop: wait for new connection/requests
	while (1) {
	
		pthread_mutex_lock(&full_queue_cond_mutex);
		while (check_if_queue_is_full()) {

			pthread_cond_wait(&full_queue_cond_var, &full_queue_cond_mutex);
		}
		pthread_mutex_unlock(&full_queue_cond_mutex);

		// wait for incomming connection
		if ((new_request.fd = accept(socket_fd, (struct sockaddr *)&client_addr, &clen)) == -1) {
			ERROR("accept()");
		}

		// get request arrival time.
		if (clock_gettime(CLOCK_REALTIME, &new_request.connection_start) == -1) {

			perror("clock_gettime()");
			exit(1);
		}

		// got connection, serve request
		fprintf(stderr, "(Info) main: Got connection from '%s'\n", inet_ntoa(client_addr.sin_addr));

		pthread_mutex_lock(&enqueue_mutex);
		enqueue(new_request);
		pthread_mutex_unlock(&enqueue_mutex);

		pthread_mutex_lock(&empty_queue_cond_mutex);
		if (!check_if_queue_is_empty()) {

			pthread_cond_broadcast(&empty_queue_cond_var);
		}
		pthread_mutex_unlock(&empty_queue_cond_mutex);
	}

	return 0; 
}