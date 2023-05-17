
#include <collector.h>

#define SOCKNAME "./farm.sck"

//globali per essere utilizzati nelle funzioni atexit
int socketServer;
int fdc;

extern treeNodePtr_t albero;

/* li ignoro con la maschera prima della fork
//ignora tutti i segnali
void ignoraSegnali(){
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
    CHECK_EQ(sigaction(SIGHUP,&sa, NULL), -1, "Sigaction SIGHUP: ")
    CHECK_EQ(sigaction(SIGINT,&sa, NULL), -1, "Sigaction SIGINT: ")
    CHECK_EQ(sigaction(SIGQUIT,&sa, NULL), -1, "Sigaction SIGQUIT: ")
    CHECK_EQ(sigaction(SIGTERM,&sa, NULL), -1, "Sigaction SIGTERM: ")
    CHECK_EQ(sigaction(SIGPIPE,&sa, NULL), -1, "Sigaction SIGPIPE: ")
    CHECK_EQ(sigaction(SIGUSR1,&sa, NULL), -1, "Sigaction SIGUSR1: ")
}
*/

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

//si occupa della socket dalla creazione alla connessione, 
//aggiungendo in exit le funzioni necessarie alla chiusura dei fd e cancellazione del file
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

void leggi(){
	int msgDim;
    long r = 0;
    //char buffer[PATHNAME_MAX_DIM];
    char* buffer = NULL;
    msg_t* nodino = NULL;

    //msgDim = 10;
    //fprintf(stderr, "lETTO msgDim %ld\n", msgDim);
    //CHECK_NULL((buffer = malloc( msgDim)), "malloc buffer") //sizeof(char) *
    //fprintf(stderr, "allocato  %ld in %s\n", msgDim, buffer);
    //free(buffer);

    do{
        CHECK_NULL((nodino = malloc(sizeof(msg_t))), "malloc nodino")

        CHECK_EQ((readn(fdc, &msgDim, sizeof(int))), -1, "readn1")
        
        if(msgDim > 0){
            CHECK_NULL((buffer = malloc( msgDim)), "malloc buffer") //sizeof(char) *
            CHECK_EQ((readn(fdc, buffer, msgDim)), -1, "readn2")
            
            CHECK_NULL((nodino->filePath = malloc(sizeof(char)*msgDim)), "malloc nodinoPath")
            strncpy(nodino->filePath, buffer, msgDim);
            free(buffer);
            buffer = NULL;

            //potrei gestire l'errore deallocando la malloc del nodino sopra
            CHECK_EQ((readn(fdc, &r, sizeof(long))), -1, "readn3")
            
            nodino->result = r;
            CHECK_EQ(inserisciNodo(nodino), -1, "inserisciNodo")

            //fprintf(stderr, "%d\t%s\t%ld\n", msgDim, buffer, r);
        }
        else{
            if(msgDim == -2){
                stampaAlbero(albero);
            }
        }
    }while(msgDim != -1);
    free(nodino);
}

/*
int main(int argc, char* argv[]){
    ignoraSegnali();

    creaSocketServer();

    ATEXIT(dealloca)
    
    msg_t* messaggio = NULL;
    int pathDim = 0;
    char* buffer = NULL;
    
    //riceve messaggi

    //while(nameDim > 0)

    stampaAlbero();
    //dealloca già tramite atexit

    return 0;
}
*/
