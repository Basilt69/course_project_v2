#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#define MEM_BAR __sync_synchronize()
#define MAX_THREAD_CNT 20UL

unsigned char ticket[MAX_THREAD_CNT];
unsigned char entering[MAX_THREAD_CNT];

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

node_t* head = NULL;
node_t* tail = NULL;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

void* handle_connection(void* p_client_socket);
int check(int exp, const char* msg);

void* thread_function(void* arg);

void lock_thread(size_t thread_id);
void unlock_thread(size_t thread_id);
bool is_interrupted = false;

void enqueue(int* client_socket);
int* dequeue();

void handler(int sig, siginfo_t* b, void* c) {}

int main(int argc, char **argv) {
    struct sigaction act;
    memset(&act, 0, sizeof (act));

    act.sa_flags = SA_SIGINFO & ~SA_RESTART;
    act.sa_sigaction = &handler;

    if (sigaction(SIGINT, &act, NULL) == -1 || sigaction(SIGQUIT, &act, NULL) == -1) {
        perror("Sigaction failed");
        return -1;
    }

    memset((void *)ticket, 0, sizeof(ticket));
    memset((void *)entering,0,sizeof(entering));

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == SOCKET_ERROR) {
        perror("Failed to create socket.");
        return -1;
    }

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

    for (size_t thread_id = 0; thread_id < THREAD_POOL_SIZE; thread_id++) {
        pthread_create(&thread_pool[thread_id], NULL, thread_function, (void*)(thread_id));
        if (&thread_pool[thread_id] == NULL) {
            printf("Error creating thread");
            return 1;
        }
    }

    while (true) {
        printf("Waiting for connections...\n");

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

        printf("Accepted connection!\n");

        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;

        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_cond_broadcast(&condition_var);
        pthread_mutex_unlock(&mutex);
    }

    printf("Finishing main thread\n");

    pthread_mutex_lock(&mutex);
    is_interrupted = true;
    pthread_cond_broadcast(&condition_var);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(thread_pool[i],NULL);
    }

    return 0;
}

int check(int exp, const char *msg){
    if (exp == SOCKET_ERROR){
        perror(msg);
        exit(1);
    }
    return exp;
}

void* thread_function(void* arg) {
    while (true) {
        size_t thread_id = (size_t)(arg);

        printf("Starting thread %lu\n", thread_id);

        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&condition_var, &mutex);

        if (is_interrupted) {
            pthread_mutex_unlock(&mutex);
            printf("Finishing thread %lu\n", thread_id);
            return NULL;
        }

        pthread_mutex_unlock(&mutex);
        int* pclient = dequeue();

        if (pclient != NULL) {
            lock_thread(thread_id);
            printf("Thread %lu entered critical section\n", thread_id);
            handle_connection(pclient);
            unlock_thread(thread_id);
            printf("Thread %lu left critical section\n\n\n", thread_id);
        }
    }
}

void* handle_connection(void* p_client_socket) {
    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    char buffer[BUF_SIZE];
    size_t bytes_read;
    size_t msgsize = 0;
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

void enqueue(int* client_socket) {
    node_t *newnode = malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    newnode->next=NULL;
    if (tail == NULL){
        head = newnode;
    }
    tail = newnode;
}

int* dequeue() {
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

void lock_thread(size_t thread_id) {
    //Before getting the ticket number "selecting" variable is set true
    printf("We entered lock function %lu\n", thread_id);

    entering[thread_id] = 1;
    MEM_BAR;
    //Memory barrier applied
    int max_num = 0;
    // Finding maximum ticket value among current threads
    for (size_t i = 0; i < MAX_THREAD_CNT; ++i) {
        int current = ticket[i];
        if (current > max_num) {
            max_num = current;
        }
    }
    ticket[thread_id] = 1 + max_num;
    //Alloting new ticket value as maximum +1
    entering[thread_id] = 0;
    MEM_BAR;
    //ENTRY Section starts
    for (size_t i = 0; i < MAX_THREAD_CNT; i++) {
        //Applying the bakery algorithm conditions
        if (i != thread_id) {
            while (entering[i] == 1) {
                sched_yield();
            }
            while (ticket[i] != 0 && (ticket[thread_id] > ticket[i] ||
                                      (ticket[thread_id] == ticket[i] && thread_id > i))) {
                sched_yield();
            }
        }

    }
}

void unlock_thread(size_t thread_id){
    MEM_BAR;
    printf("We unlocked thread %lu\n", thread_id);
    ticket[thread_id] = 0;
}