// a new thread for every conncection
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <sys/un.h>


#define SERVERPORT 8989
#define BUFSIZE 4096
#define SOCKETERROR (-1)
#define SERVER_BACKLOG 100
#define THREAD_POOL_SIZE 12

pthread_t thread_pool[THREAD_POOL_SIZE];

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

void* handle_connection(void* p_client_socket);
int check(int exp, const char *msg);
void* thread_function(void *arg);

int main(int argc, char **argv) {

    int server_socket, addr_size;
    int client_socket;
    SA_IN server_addr, client_addr;

    // create a bunch of threads to handle future connections
    for (int i=0; i < THREAD_POOL_SIZE; i++) {
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
