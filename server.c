// a new thread for every conncection
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /*importing POSIX Operating System API library*/
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h> /*importing  the thread library*/
#include "myqueue.h"
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <sys/un.h>


#define SERVERPORT 8989
#define BUFSIZE 4096
#define SOCKETERROR (-1)
#define SERVER_BACKLOG 100
#define THREAD_POOL_SIZE 12
#define MEMBAR __sync_synchronize() /*memory barrier instruction*/

volatile int num[THREAD_POOL_SIZE]; /*volatile prevents the compiler from applying any optimizations*/
volatile int selecting[THREAD_POOL_SIZE];
volatile int res;

void lock_thread(int thread){
    // Before getting the ticket number  "selecting" variable is set true

}

pthread_t thread_pool[THREAD_POOL_SIZE];

node_t* head = NULL;
node_t* tail = NULL;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

void* handle_connection(void* p_client_socket);
int check(int exp, const char *msg);
void* thread_function(void *arg);
void enqueue(int *client_socket);
int* dequeue();


int main(int argc, char **argv) {

    int server_socket, addr_size;
    int client_socket;
    SA_IN server_addr, client_addr;
    memset((void *)num,0,sizeof(num));
    memset((void *)selecting,0,sizeof(selecting));
    res=0;

    //Declaring the thread variables
    pthread_t thread_pool[THREAD_POOL_SIZE];

    // create a bunch of threads to handle future connections
    for (int i=0; i < THREAD_POOL_SIZE; i++) {
        //""thread body is the thread routine
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }

    check((server_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to create socket.");

    // initialize the address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVERPORT);

    check(bind(server_socket, (SA*)&server_addr, sizeof(server_addr)), "Bind failed!");
    check(listen(server_socket, SERVER_BACKLOG), "Listen failed!");

    while(true){
        printf("Waiting for connections...\n");
        //wait for and eventually accept an incoming connection
        addr_size = sizeof(SA_IN);
        check(client_socket = accept(server_socket, (SA*)&client_addr, (socklen_t*)&addr_size), "accept failed");
        printf("Connected!\n");

        // put the connection somewhere so that an available thread can find it
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;


        enqueue(pclient);

    }

    for (int i=0; i<THREAD_POOL_SIZE;i++) {
        //Reaping the resources used by all threads once their task is completed
        pthread_join(thread_pool[i],NULL);
    }


    return 0;
}

int check(int exp, const char *msg){
    if (exp == SOCKETERROR){
        perror(msg);
        exit(1);
    }
    return exp;
}

void * thread_function(void *arg){
    while(true){
        int *pclient;

        pclient = dequeue();

        if (pclient != NULL) {
            //we have a connection
            handle_connection(pclient);
        }
    }
}

void * handle_connection(void* p_client_socket) {
    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    char buffer[BUFSIZE];
    size_t bytes_read;
    int msgsize = 0;
    char actualpath[PATH_MAX+1];

    // read the client's message -- the name of the file to read
    while((bytes_read = read(client_socket, buffer+msgsize, sizeof(buffer)-msgsize-1)) > 0) {
        msgsize += bytes_read;
        if (msgsize > BUFSIZE-1 || buffer[msgsize-1] == '\n') break;
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
    while((bytes_read = fread(buffer, 1, BUFSIZE, fp)) > 0){
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
