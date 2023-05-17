#ifndef COLLECTOR
#define COLLECTOR

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <tree.h>
#include <myLibrary.h>

#define SOCKNAME "./farm.sck"

void chiudiSocketServer();
void cancellaFileSocket();
void chiudiConnessione();
void creaSocketServer();

void leggi();

#endif