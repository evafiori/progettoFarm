#include <master.h>

#define SOCKNAME "./farm.sck"

sigset_t mask; //maschera dei segnali
pthread_t idGestoreSegnali = -1; //durante i test: se == -1 -> thread non creato

int terminaCodaFlag = 0; //segnala di non inserire altro in coda
int terminaGestoreFlag = 0; //segnala al master che il gestore è stato terminato da un segnale e non è necessaria la kill
int stampaFlag = 0; //segnala ai thread di lanciare un messaggio di stampa

int socketClient; //fd lato client
pthread_mutex_t mtxSocket = PTHREAD_MUTEX_INITIALIZER; //mutex per la scrittura sulla socket

extern BQueue_t *pool; //pool del threadpool 
extern lPtr fileBinari; //lista dei file binari passati come argoment (non in una directory)

//thread che cattura tutti i segnali (stampa e li terminazione) e li gestisce attivando i flag relativi
//esce con la ricezione di un segnale
//se questo non arriva durante l'esecuzione, sarà una kill a terminare il gestore: ricezione segnale SIGTERM
void* gestoreSegnali(void* mask){
    int sig;
    //il ciclo è necessario per poter ricevere più volte il segnale SIGUSR1
    while(1){ 
        CHECK_NEQ((errno = sigwait((sigset_t*) mask, &sig)), 0, "sigwait: ")
        
        switch(sig){
            case SIGHUP:
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
                terminaCodaFlag = 1;
                terminaGestoreFlag = 1;
                pthread_exit(NULL);
            case SIGUSR1:
                stampaFlag = 1;
                break;
            default: ;
        }
    }
}

//maschera i segnali a tutti i thread
//la maschera è ereditata dopo la fork => questa maschera è valida sia per MasterWorker che per Collector
void mascheraSegnali(){
    struct sigaction s;

    memset(&s, 0, sizeof(s)); 
    // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket
    s.sa_handler = SIG_IGN;
    CHECK_EQ(sigaction(SIGPIPE, &s, NULL), -1, "sigaction")

    //aggiungo i segnali alla maschera
    CHECK_EQ(sigemptyset(&mask), -1, "sigemptyset: ") 
    CHECK_EQ(sigaddset(&mask, SIGHUP), -1, "SIGHUP")
    CHECK_EQ(sigaddset(&mask, SIGINT), -1, "SIGINT")
    CHECK_EQ(sigaddset(&mask, SIGQUIT), -1, "SIGQUIT")
    CHECK_EQ(sigaddset(&mask, SIGTERM), -1, "SIGTERM")
    CHECK_EQ(sigaddset(&mask, SIGUSR1), -1, "SIGUSR1")
    
    //blocco segnali della maschera a tutti i thread
    CHECK_NEQ((errno = pthread_sigmask(SIG_BLOCK, &mask, NULL)), 0, "pthread_sigmask: ")
    
}

//chiude la connessione socket lato client
void chiudiSocketClient(){
    CLOSE(socketClient, "Close socket client: ")
}

////si occupa della socket lato client dalla creazione alla connessione
void creaSocketClient(){
    struct sockaddr_un sa;
    
    CHECK_EQ((socketClient = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "Socket")
    ATEXIT(chiudiSocketClient)

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    CHECK_EQ(myConnect(socketClient, (struct sockaddr *) &sa, sizeof(sa)), -1, "Connect")
}

//scansiona la directory passata con l'opzione -d e mette in coda concorrente i file regolari all'interno 
void scanDir (char* directory){
	DIR * mydir = NULL;
    char* path_stat = NULL;
	struct dirent* f = NULL; //contiene #i-node e nome del file
	struct stat buf;
    int sizePath, sizeDir, sizeName;
	
	CHECK_NULL((mydir = opendir(directory)), "Aprendo la directory: ")

	//finché trovo file nella directory e non è attivo il flag di terminazione
	while((errno = 0),(f = readdir(mydir)) != NULL && terminaCodaFlag == 0){

		if(strncmp(f->d_name, ".", 1) != 0 && strncmp(f->d_name, "..", 2) != 0 ){

            //calcolo dimensione pathname
            sizeName = myStrnlen(f->d_name, PATHNAME_MAX_DIM);
            sizeDir = myStrnlen(directory, PATHNAME_MAX_DIM);
            sizePath = sizeDir + sizeName + 2; //per la barra e lo \0

            if(sizePath < PATHNAME_MAX_DIM){
                //creazione del pathname
                CHECK_NULL((path_stat = malloc(sizeof(char) * sizePath)), "malloc")
                strncpy(path_stat, directory, sizeDir);
                path_stat[sizeDir] = '/';
                path_stat[sizeDir+1] = '\0';
                if(myStrncat(path_stat, f->d_name, sizePath) == -1){
                    errno = ERANGE;
                    perror("Pathname troppo lungo: ");
                }
                else{
                    CHECK_EQ(stat(path_stat, &buf), -1, "Lettura informazioni file: ")
                    if(S_ISDIR(buf.st_mode)){
                        //chiamata ricorsiva sulla nuova dir trovata: path_stat
                        scanDir(path_stat);
                        free(path_stat);
                    }
                    else{
                        if(verificaReg(path_stat)){
                            //inserimento in boundedqueue di path_stat
                            CHECK_EQ(push(pool, path_stat), -1, "push")
                            nanosleep(&(pool->delay), NULL); //delay specificato o 0
                        }
                        else{
                            fprintf(stderr, "File %s non regolare.\n", path_stat);
                        }
                    }
                }
            }
            else{
                errno = ERANGE;
                perror("Pathname troppo lungo: ");
            }
		}
	}
	CHECK_NEQ(errno, 0, "Lettura directory incompleta: ")
	CHECK_EQ(closedir(mydir), -1, "Chiudendo la directory: ")
}

//scansione lista e inserimento in coda
void pushFiles(){
    lPtr temp = NULL;
    while(fileBinari != NULL && terminaCodaFlag == 0){
        CHECK_EQ(push(pool, fileBinari->file), -1, "push")
        nanosleep(&(pool->delay), NULL); //delay specificato o 0
        temp = fileBinari->next;
        free(fileBinari);
        fileBinari = temp;
    }

}