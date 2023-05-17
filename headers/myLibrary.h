#ifndef MY_LIB
#define MY_LIB

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h> 
#include <dirent.h> 

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#define PATHNAME_MAX_DIM 256 //lunghezza massima compreso '\0'

#define CHECK_NULL(p, str) if(p == NULL){ perror(str); exit(EXIT_FAILURE); }
#define CHECK_EQ(X, val, str) if((X) == val){ perror(str); exit(EXIT_FAILURE); }
#define CHECK_NEQ(X, val, str) if((X) != val){ perror(str); exit(EXIT_FAILURE); }

#define CLOSE(fd, str) CHECK_EQ(close(fd), -1, str)

#define ATEXIT(func) if(atexit(func) != 0){ fprintf(stderr, "cannot set exit function\n"); exit(EXIT_FAILURE); }

#define LOCK(mtx) if ((errno = pthread_mutex_lock(&mtx)) != 0){ perror("Mutex lock"); exit(EXIT_FAILURE); }
#define UNLOCK(mtx) if ((errno = pthread_mutex_unlock(&mtx)) != 0){ perror("Mutex unlock"); exit(EXIT_FAILURE); }
#define WAIT(c, m) if ((errno = pthread_cond_wait(&c, &m)) != 0) { perror("Cond wait"); exit(EXIT_FAILURE); }
#define SIGNAL(c) if ((errno = pthread_cond_signal(&c)) != 0) { perror("Cond signal"); exit(EXIT_FAILURE); }
#define BROADCAST(c) if((errno = pthread_cond_broadcast(&c)) != 0) { perror("Cond broadcast"); exit(EXIT_FAILURE); }

#define UNIX_PATH_MAX 108

//int nanosleep(const struct timespec *req, struct timespec *rem);
//extern int nanosleep (const struct timespec *__requested_time, struct timespec *__remaining);

int isNumber(const char* s);

int verificaReg(char* pathname);
int verificaDir(char* pathname);

size_t strnlen(const char *s, size_t maxlen);
size_t myStrnlen(char* str, size_t max);
int myStrncat(char* s1, char* s2, size_t max); //se non la usi toglila

int readn (int fd, void* buf, size_t n);
int writen (int fd, void* buf, size_t n);

int myConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

#endif