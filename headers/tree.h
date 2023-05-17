#ifndef TREE
#define TREE

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <string.h>
#include <errno.h>

#include <myLibrary.h>

typedef struct messaggio{
    long result;
    char* filePath;
}msg_t;

typedef struct nodo{
    struct nodo* sx;
    struct nodo* dx;
    msg_t* msg;
}treeNode_t;

typedef treeNode_t* treeNodePtr_t;

int inserisciNodo (msg_t* temp);

void stampaAlbero(treeNodePtr_t scorri);

void deallocaAlbero (treeNodePtr_t* a);
void dealloca();

#endif