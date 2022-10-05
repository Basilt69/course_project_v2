#include <limits.h>
#include <pthread.h> /*importing  the thread library*/
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /*importing POSIX Operating System API library*/
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <arpa/inet.h>


#include "myqueue.h"



#define SERVER_PORT 8989
#define BUF_SIZE 4096
#define SOCKET_ERROR (-1)
#define SERVER_BACKLOG 100
#define THREAD_POOL_SIZE 20
#define MEMBAR __sync_synchronize() /*memory barrier instruction*/
#define MAXTHREADCNT 20

// max number of threads
//const int MAXTHREADCNT=20;
// real number of threads
int threadcnt=0;

typedef unsigned char byte; // creating of byte type

//volatile byte ticket[MAXTHREADCNT]; /*volatile prevents the compiler from applying any optimizations*/
//volatile byte entering[MAXTHREADCNT];

byte ticket[MAXTHREADCNT]; /*volatile prevents the compiler from applying any optimizations*/
byte entering[MAXTHREADCNT];

node_t* head = NULL;
node_t* tail = NULL;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

void* handle_connection(void* p_client_socket);
int check(int exp, const char *msg);

void* thread_function(void *arg);

void lock_thread(int thread_id);
void unlock_thread(int thread_id);

void enqueue(int *client_socket);
int* dequeue();


void handler(int a, siginfo_t *b, void *c) {}

int main(int argc, char **argv) {
    pthread_t thread_id; //thread id

    // Parameters, threads and signal handler
    struct sigaction act;
    memset(&act, 0, sizeof (act));

    act.sa_flags = SA_SIGINFO & ~SA_RESTART;
    act.sa_sigaction = &handler;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("Sigaction failed");
        return -1;
    }

    /*memset((void *)num, 0, sizeof(num));
    memset((void *)selecting,0,sizeof(selecting));*/


    memset((void *)ticket, 0, sizeof(ticket));
    memset((void *)entering,0,sizeof(entering));

    //Declaring the thread variables
    pthread_t thread_pool[THREAD_POOL_SIZE];

    // Creating server socket to handle client connections(requests)
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == SOCKET_ERROR) {
        perror("Failed to create socket.");
        return -1;
    }

    // initialize the address struct
    SA_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (SA*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("Bind failed");
        return -1;
    }

    if (listen(server_socket, SERVER_BACKLOG) == SOCKET_ERROR) {
        perror("Listen failed");
        return -1;
    }

    // Create and listen to the client sockets(+numerate them)
    while (true) {
        printf("Waiting for connections...\n"); //wait for and eventually accept an incoming connection


        SA_IN client_addr;
        size_t addr_size = sizeof(SA_IN);
        memset(&client_addr, 0, sizeof (client_addr));
        int client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size);
        if (client_socket == SOCKET_ERROR) {
            if (errno == EINTR) {
                printf("\nInterrupting\n");
                break;
            }
            perror("Accept failed");
            return -1;
        }

        printf("Connected!\n");

        // put the connection somewhere so that an available thread can find it
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;


        //long thread = (long) argc;


        //lock_thread(thread);
        threadcnt++;
        // creating queque of sockets and counting them
        printf("This is threadcnt %d in main \n", threadcnt);

        printf("This is plcient in main before enqueue %d\n", pclient);

        enqueue(pclient);
        //printf("This is threadcnt %d\n", threadcnt);

        //unlock_thread(thread);

        // creating of threads to handle connections
        for (int i=0; i < threadcnt; i++) {
            //""thread body is the thread routine
            printf("We entered thread create %d\n", i);
            pthread_create(&thread_pool[i], NULL, thread_function, (void *)((long)i));
            if (&thread_pool[i] == NULL) {
                printf("Error create thread");
                return 1;
            }
        }
        for (int i=0; i<threadcnt;i++) {
            //Reaping the resources used by all threads once their task is completed
            pthread_join(thread_pool[i],NULL);
        }

    }




    /*


    // create a bunch of threads to handle future connections
    for (int i=0; i < THREAD_POOL_SIZE; i++) {
        //""thread body is the thread routine
        pthread_create(&thread_pool[i], NULL, thread_function, (void *)((long)i));
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == SOCKET_ERROR) {
        perror("Failed to create socket.");
        return -1;
    }

    // initialize the address struct
    SA_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (SA*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("Bind failed");
        return -1;
    }

    if (listen(server_socket, SERVER_BACKLOG) == SOCKET_ERROR) {
        perror("Listen failed");
        return -1;
    }

    while (true) {
        printf("Waiting for connections...\n"); //wait for and eventually accept an incoming connection


        SA_IN client_addr;
        size_t addr_size = sizeof(SA_IN);
        memset(&client_addr, 0, sizeof (client_addr));
        int client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size);
        if (client_socket == SOCKET_ERROR) {
            if (errno == EINTR) {
                printf("\nInterrupting\n");
                break;
            }
            perror("Accept failed");
            return -1;
        }

        printf("Connected!\n");

        // put the connection somewhere so that an available thread can find it
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;


        long thread = (long) argc;


        //lock_thread(thread);
        enqueue(pclient);
        threadcnt++;

        //unlock_thread(thread);
    }


    for (int i=0; i<THREAD_POOL_SIZE;i++) {
        //Reaping the resources used by all threads once their task is completed
        pthread_join(thread_pool[i],NULL);
    }
*/
    return 0;
}



int check(int exp, const char *msg){
    if (exp == SOCKET_ERROR){
        perror(msg);
        exit(1);
    }
    return exp;
}


void * thread_function(void *arg){
    /*while(true){
        int *pclient;
        long thread_id = (long) arg;


        pclient = dequeue();
        printf("Pclient in dequeue %d thread_fucntion", pclient);
        printf("This is thread_id in thread function %d\n", thread_id);

        if (pclient != NULL) {
            //we have a connection
            lock_thread(thread_id);
            printf("Thread %d entered critical section\n", (int )thread_id);
            handle_connection(pclient);
            unlock_thread(thread_id);
            printf("Thread %d left critical section\n", (int)thread_id);
            return NULL;
        }*/
    int *pclient;
    long thread_id = (long) arg;


    pclient = dequeue();
    printf("Pclient in dequeue %d thread_fucntion\n", pclient);
    printf("This is thread_id in thread function %d\n", thread_id);

    if (pclient != NULL) {
        //we have a connection
        lock_thread(thread_id);
        printf("Thread %d entered critical section\n", (int )thread_id);
        handle_connection(pclient);
        unlock_thread(thread_id);
        printf("Thread %d left critical section\n\n\n", (int)thread_id);
        return NULL;
    }
    }



void * handle_connection(void* p_client_socket) {
    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    char buffer[BUF_SIZE];
    size_t bytes_read;
    int msgsize = 0;
    char actualpath[PATH_MAX+1];

    // read the client's message -- the name of the file to read
    while((bytes_read = read(client_socket, buffer+msgsize, sizeof(buffer)-msgsize-1)) > 0) {
        msgsize += bytes_read;
        if (msgsize > BUF_SIZE-1 || buffer[msgsize-1] == '\n') break;
    }
    check(bytes_read,"recv error");
    buffer[msgsize-1] =0; // null terminate the message and remove the \n

    printf("REQUEST: %s\n", buffer);
    fflush(stdout);

    //validity check
    if (realpath(buffer, actualpath) == NULL){
        printf("ERROR(bad path): %s\n",buffer);
        close(client_socket);
        return NULL;
    }

    // read file and send its contents to client
    FILE *fp = fopen(actualpath, "r");
    if (fp == NULL){
        printf("ERRPR(open): %s\n",buffer);
        close(client_socket);
        return NULL;
    }
    sleep(1);

    // read file contents and send them to client
    while((bytes_read = fread(buffer, 1, BUF_SIZE, fp)) > 0){
        printf("sending %zu bytes\n",bytes_read);
        write(client_socket, buffer,bytes_read);
    }
    close(client_socket);
    fclose(fp);
    printf("closing connection\n");
    return NULL;
}

void enqueue(int *client_socket){

    node_t *newnode = malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    printf("Threadcnt %d\n", threadcnt);
    newnode->next=NULL;
    if (tail == NULL){
        head = newnode;
    }
    tail = newnode;
}

// returns NULL if the queue is empty
// returns the pointer to a client_socket, if there is one to get
int* dequeue(){
    if (head == NULL) {
        return NULL;
    } else {
        int *result = head->client_socket;
        node_t *temp = head;
        head = head->next;
        if (head == NULL) {tail = NULL;}
        free(temp);
        return result;
    }
}



void lock_thread(int thread_id) {
    //Before getting the ticket number "selecting" variable is set true
    printf("We enetered lock fucntion in lock_thread\n");

    entering[thread_id] = 1;
    MEMBAR;
    //Memory barrier applied
    int max_num = 0;
    // Finding maximum ticket value among current threads
    for (int i = 0; i < threadcnt; ++i) {
        int current = ticket[i];
        if (current > max_num) {
            max_num = current;
        }
        //max_num = ticket > max_num ? ticket:max_num;
    }
    ticket[thread_id] = 1 + max_num;
    //Alloting new ticket value as maximum +1
    entering[thread_id] = 0;
    MEMBAR;
    //ENTRY Section starts
    for (int i = 0; i < threadcnt; i++) {
        //Applying the bakery algorithm conditions
        if (i != thread_id) {
            while (entering[i] == 1) {
                sched_yield();
                printf("THIS is entering[i]\n", entering[i]);
            }
            while (ticket[i] != 0 && (ticket[thread_id] > ticket[i] ||
                                      (ticket[thread_id] == ticket[i] && thread_id > i))) {
                sched_yield;
                printf("This is ticket[i] %d\n, this is ticket[thread_id] %d\n, this is i %d\n", ticket[i],
                       ticket[thread_id], i);

            }
        }

    }
    printf("We left lock fucntion in lock_thread\n");
}

// EXIT Section
void unlock_thread(int thread_id){
    MEMBAR;
    // Return the taken ticket number
    printf("We enetered unlock fucntion in unlock_thread\n");
    ticket[thread_id]=0;
    printf("We left unlock fucntion in unlock_thread\n");
}





/*
void * thread_function(void *arg){
    while(true){
        int *pclient;
        long thread = (long) arg;
        lock_thread(thread);

        pclient = dequeue();

        if (pclient != NULL) {
            //we have a connection
            printf("Thread %d entered critical section\n", (int )thread);
            handle_connection(pclient);
            unlock_thread(thread);
            printf("Thread %d left critical section\n", (int)thread);
            return NULL;
        }
    }
}

 */

/*
void lock_thread(int thread) {
    //Before getting the ticket number "selecting" variable is set true
    selecting[thread] = 1;
    MEMBAR;
    //Memory barrier applied
    int max_num = 0;
    // Finding maximum ticket value among current threads
    for (int i=0; i<THREAD_POOL_SIZE;++i){
        int ticket = num[i];
        max_num = ticket > max_num ? ticket:max_num;
    }
    //Alloting new ticket value as maximum +1
    num[thread] = max_num + 1;
    MEMBAR;
    selecting[thread]=0;
    MEMBAR;
    //ENTRY Section starts
    for (int other=0; other<THREAD_POOL_SIZE;++other){
        //Applying the bakery algorithm conditions
        while (selecting[other]){
            sched_yield();
            printf("THIS is selecting[other]\n",selecting[other]);
        }
        MEMBAR;
        while (num[other] !=0 && (num[other] <num[thread] || (num[other] == num[thread] && other < thread))){

            printf("This is num[other] %d\n, this is num[thread] %d\n, this is other %d\n, this is thread %d\n", num[other], num[thread], other, thread);

        }
    }

}
*/

