#ifndef LIST
#define LIST

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <myLibrary.h>

typedef struct list{
    struct list* next;
    char* file;
}l;

typedef l* lPtr;

lPtr inserisciTesta (lPtr e, char* f);

//lPtr inserisciCoda (lPtr *e, char* f);

char* estraiTesta (lPtr *e);

lPtr eliminaTesta (lPtr *e);

void stampaLista(lPtr e);

lPtr svuotaLista(lPtr *e);

#endif