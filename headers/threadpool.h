#ifndef THREADPOOL
#define THREADPOOL

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include <myLibrary.h>

/*
typedef struct Node{
    char* filePath;
    int pathLen;
}Node_t;
*/

typedef struct BQueue {
    char** queue;
    int   head;
    int   tail;
    int   qsize;
    int   qlen;
    struct timespec delay;
    pthread_mutex_t  m;
    pthread_cond_t   cfull;
    pthread_cond_t   cempty;
    pthread_t      * threads; // array di worker id
    int numthreads;           // numero di thread (size dell'array threads)
    int threadAttivi;         // numero di task attualmente in esecuzione 
    //^non so se davvero mi interessa
    //protocollo di uscita fuori dal threadpool perché di interesse anche del master
} BQueue_t;

char* pop(BQueue_t *q);
int push(BQueue_t *q, char* data);
long risultato(char * p);

void *worker_thread(void *threadpool);

int freePoolResources(BQueue_t *pool);
void destroyThreadPool();
int createThreadPool(int num, int size, int del);

#endif