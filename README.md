# Multithreaded Key-Value Server
A simple multithreaded server storing and retrieving key-value pairs.\
The server recieves a mixed sequence of PUT/GET requests by the client. A PUT request saves a new Key-Value pair. A GET request retrieves an existing value based on a key.\
Upon termination, it provides a basic statistical analysis of service time.

## Workflow
<p>Using the Server-Client model, the client defines the Key-Value pair that will be stored or the Key based on which the corresponding value will be retrieved. He communicates with the server through a network socket. The server recovers the pair or key from the incoming request, accesses the storage to save or retrieve a pair and responds to the client.\
Finally, the client shows the result on screen.</p>

<p>The server is based on the Producer-Consumer model. A producer thread is waiting for incoming requests while a predefined number of consumer threads is responsible for serving them. When the server recieves a new request, the producer thread adds the connection descriptor to a shared FIFO queue. Then, a consumer thread extracts the descriptor from the queue and carries out the PUT or GET request.
The consumer threads remain idle until a new request reaches the server. Upon arrival of the request, the producer thread prepairs a structure (the aforementioned *connection descriptor*) containing the file descriptor of the socket connection plus, the connection arrival time. The structure is then added to the shared queue and the consumer threads are notified to carry on with their task.
The queue can be accessed simultaneously by one consumer thread and one producer thread. The consumer threads can perform one PUT request to the storage at a time while they can perform multiple GET requests simultaneously. When each consumer thread has finished it's job, it calculates two values which are then added to two global variables. The values address the total time it took to complete the request and the total time it remained in the queue.</p>

<p>To test the multithreaded implementation of the server, the client had to be able to send multiple requests at a time so, a multithreaded implementation of him was necessary. Each client thread randomly creates one or more PUT or GET requests with a random Key and Value which then is sent to the server. Upon completion of the requests, the client shows the elapsed time.</p>

# Libraries
The multithreaded implementation is based on Linux's POSIX threads.\
[KISSDB](https://github.com/adamierymenko/kissdb) is used for the Key-Value storage.

# Motive
Assigned during *Operating Systems* course of University of Ioannina during the spring semester of 2020.
The single threaded implementation has been provided by [S. Anastasiadis](http://www.cse.uoi.gr/~stergios/) and [G. Kappes](http://www.cs.uoi.gr/~gkappes/).

# How to run
Compile with any major version of gcc. The oldest tested version is 7.5.0.\
Copy the .c, .h and Makefile to your directory of choice, navigate there and compile with make:
```
make all
```
To run the server type:
```
./server
```
To run the client, in a new terminal, type:
```
./client
```
To terminate the server, on server's terminal send a SIGSTP signal with `Ctrl + Z`.
The statistical analysis of service time is then calculated and presented to screen.
