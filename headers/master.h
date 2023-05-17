#ifndef MASTER
#define MASTER

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h> 
#include <dirent.h> 

#include <myLibrary.h>
#include <threadpool.h>
#include <list.h>

void* gestoreSegnali(void* mask);
void mascheraSegnali();

void chiudiSocketClient();
void creaSocketClient();

void scanDir(char* directory);
void pushFiles();

#endif