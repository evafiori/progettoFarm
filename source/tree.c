
#include <tree.h>

treeNodePtr_t albero = NULL;

//restituisce 1 se tutto okay, -1 altrimenti con errno settato
int inserisciNodo (msg_t* temp){
    if(temp == NULL){
        errno = EINVAL;
        return -1;
    }

    treeNodePtr_t n = NULL;
    CHECK_NULL((n = malloc(sizeof(treeNode_t))), "malloc")
    n->dx = NULL;
    n->sx = NULL;
    n->msg = temp;

    if(albero == NULL){
        albero = n;
    }
    else{
        treeNodePtr_t scorri = albero;
        int inserito = 0;
        while(inserito == 0){
            if(scorri->msg->result > temp->result){
                if(scorri->sx != NULL){
                    scorri = scorri->sx;
                }
                else{
                    scorri->sx = n;
                    inserito = 1;
                }
            }
            else{
                if(scorri->dx != NULL){
                    scorri = scorri->dx;
                }
                else{
                    scorri->dx = n;
                    inserito = 1;
                }
            }
        }
    }
    return 1;
}

//stampa tutto l'albero in maniera ordinata e già formattata
void stampaAlbero(treeNodePtr_t scorri){
    
    if(scorri != NULL){
        stampaAlbero(scorri->sx);
        printf("%ld\t%s\n", scorri->msg->result, scorri->msg->filePath);
        stampaAlbero(scorri->dx);
    }
}

//dealloca l'intero albero, grazie a chiamate ricorsive su entrambi i rami
void deallocaAlbero(treeNodePtr_t* a){
    if(*a != NULL){
        deallocaAlbero(&((*a)->sx));
        deallocaAlbero(&((*a)->dx));
        free((*a)->msg->filePath);
        free((*a)->msg);
        free(*a);
    }
}

//è la funzione senza parametri chiamata in atexit, chiama a sua volta la funzione deallocaAlbero sopra
void dealloca(){
    deallocaAlbero(&albero);
}