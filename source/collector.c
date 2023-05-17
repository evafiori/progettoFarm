
#include <collector.h>

#define SOCKNAME "./farm.sck"

int socketServer;
int fdc;

extern treeNodePtr_t albero;

//chiude collegamento tramite filedescriptor
void chiudiSocketServer(){
    CLOSE(socketServer, "Close socket server: ")
}

//cancella il file farm.sck creato
void cancellaFileSocket(){
    unlink(SOCKNAME);
}

//chiude connessione tramite filedescriptor
void chiudiConnessione(){
    CLOSE(fdc, "Close socket connection: ")
}

//si occupa della socket lato server dalla creazione alla connessione, 
//aggiungendo in exit le funzioni necessarie alla chiusura dei fd e cancellazione del file socket
void creaSocketServer(){
    struct sockaddr_un sa;

    CHECK_EQ((socketServer = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "socket")
    ATEXIT(chiudiSocketServer)

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    CHECK_EQ(bind(socketServer, (struct sockaddr *) &sa, sizeof(sa)), -1, "Bind: ")
    ATEXIT(cancellaFileSocket)

    CHECK_EQ(listen(socketServer, 1), -1, "Listen: ")
    
    // accept può essere interrota da EINVAL
    CHECK_EQ((fdc = accept(socketServer, NULL, 0)), -1, "Accept: ")
    ATEXIT(chiudiConnessione)
}

//legge i messaggi sulla socket e li aggiunge all'albero, riconoscendo i messaggi di terminazione e di stampa
void leggi(){
	int msgDim;
    long r = 0;
    char* buffer = NULL;
    msg_t* node = NULL;

    do{
        CHECK_NULL((node = malloc(sizeof(msg_t))), "malloc node")

        //lettura dimensione messaggio
        if((readn(fdc, &msgDim, sizeof(int))) == -1){
            free(node);
            perror("readn1");
            exit(EXIT_FAILURE);
        }
        
        if(msgDim > 0){ //il messaggio indica la dimensione del messaggio ed esclude terminazione e stampa
            
            if((buffer = malloc(msgDim)) == NULL){ //sizeof(char) * msgDim: char è un byte
                free(node);
                perror("malloc buffer");
                exit(EXIT_FAILURE);
            }

            //lettura pathname file
            if((readn(fdc, buffer, msgDim)) == -1){
                free(node);
                free(buffer);
                perror("readn2");
                exit(EXIT_FAILURE);
            }
            
            if((node->filePath = malloc(sizeof(char)*msgDim)) == NULL){
                free(node);
                free(buffer);
                perror("malloc nodePath");
                exit(EXIT_FAILURE);
            }
            strncpy(node->filePath, buffer, msgDim);
            free(buffer);
            buffer = NULL;

            //lettura risultato long
            if((readn(fdc, &r, sizeof(long))) == -1){
                free(node->filePath);
                free(node);
                perror("readn3");
                exit(EXIT_FAILURE);
            }
            
            node->result = r;
            CHECK_EQ(inserisciNodo(node), -1, "inserisciNodo")
        }
        else{
            if(msgDim == -2){ //-2: messaggio di stampa
                free(node);
                stampaAlbero(albero);
            }
        }
    }while(msgDim != -1); //-1: messaggio di terminazione
    free(node);
}
