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

byte ticket[MAXTHREADCNT]; /*volatile prevents the compiler from applying any optimizations*/
byte entering[MAXTHREADCNT];

//thread_function signal handler
static volatile sig_atomic_t is_interrupted = 0;

//Declaring the thread variables
pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

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


void handler(int sig, siginfo_t *b, void *c) {
    printf("\nCaugt signal %d (%s)\n", sig, strsignal(sig));

    if (sig == SIGQUIT)
        is_interrupted = 1;
}

int main(int argc, char **argv) {
    pthread_t thread_id; //thread id

    // Parameters, threads and signal handler
    struct sigaction act;
    memset(&act, 0, sizeof (act));

    act.sa_flags = SA_SIGINFO & ~SA_RESTART;
    act.sa_sigaction = &handler;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("Sigaction failed");
        return -1;}
    if (sigaction(SIGQUIT, &act, NULL) == -1) {
        perror("Sigaction failed(quit)");
        return -1;
    }

    memset((void *)ticket, 0, sizeof(ticket));
    memset((void *)entering,0,sizeof(entering));

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

    for(int i=0; i < THREAD_POOL_SIZE; i++)
    {//""thread body is the thread routin
        pthread_create(&thread_pool[i], NULL, thread_function, (void *)((long)i));
        if (&thread_pool[i] == NULL) {
            printf("Error create thread");
            return 1;
        }}


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

        threadcnt++;

        printf("Threadcnt in main %d\n", threadcnt);
        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
        }
    for (int i=0; i<THREAD_POOL_SIZE;i++) {
            //Reaping the resources used by all threads once their task is completed
            printf("We are in thread join\n");
            pthread_join(thread_pool[i],NULL);}

    return 0;
}



int check(int exp, const char *msg){
    if (exp == SOCKET_ERROR){
        perror(msg);
        exit(1);
    }
    return exp;
}


void * thread_function(void *arg) {
    while(!is_interrupted){
        if (errno == EINTR) {
            printf("\nInterrupting\n");
            break;
        }
        int *pclient;
        long thread_id = (long) arg;
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&condition_var, &mutex);
        pclient = dequeue();
        pthread_mutex_unlock(&mutex);
        printf("This is thread_id in thread_function %d\n", thread_id);

        if (pclient != NULL) {
            //we have a connection
            lock_thread(thread_id);
            printf("Thread %d entered critical section\n", (int) thread_id);
            handle_connection(pclient);
            unlock_thread(thread_id);
            printf("Thread %d left critical section\n\n\n", (int) thread_id);
            return NULL;
}}}



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
    printf("We enetered lock fucntion %d\n", thread_id);

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
            }
            while (ticket[i] != 0 && (ticket[thread_id] > ticket[i] ||
                                      (ticket[thread_id] == ticket[i] && thread_id > i))) {
                sched_yield;
            }
        }

    }
}

// EXIT Section
void unlock_thread(int thread_id){
    MEMBAR;
    // Return the taken ticket number
    printf("We unlocked thread %d\n", thread_id);
    ticket[thread_id]=0;
}