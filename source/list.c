
#include <list.h>

//riceve come parametri il puntatore alla testa e il filename da inserire
//restituisce il puntatore alla nuova testa
lPtr inserisciTesta (lPtr e, char* f){
    size_t lung = myStrnlen(f, PATHNAME_MAX_DIM) +1; //\0
    if(lung == -1){
        perror("Dimensione pathname non valida: ");
        return e;
    }
    
    lPtr temp = malloc(sizeof(l));
    CHECK_NULL(temp, "malloc")
    if((temp->file = malloc(sizeof(char)*(lung))) == NULL){ //+1 per il terminatore di stringa
        free(temp);
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strncpy(temp->file, f, lung);

    temp->next = e;
    return temp;
}

//libera lo spazio allocato per la lista
lPtr svuotaLista(lPtr *e){
    lPtr temp = NULL;

    while(*e != NULL){
        temp = (*e)->next;
        free((*e)->file);
        free(*e);
        *e = temp;
    }

    return *e;
}

//stampa la lista passata come argomento
void stampaLista(lPtr e){
    lPtr temp = e;
    while(temp != NULL){
        printf("%s\n", temp->file);
        temp = temp->next;
    }
}
