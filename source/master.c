#include <master.h>

#define SOCKNAME "./farm.sck"

sigset_t mask;

int terminaCodaFlag = 0;
int terminaGestoreFlag = 0;
int stampaFlag = 0;

pthread_t idGestoreSegnali = -1;

int socketClient;

pthread_mutex_t mtxSocket = PTHREAD_MUTEX_INITIALIZER;

extern BQueue_t *pool;
extern lPtr fileBinari;

//fa terminare il thread gestore e libera la memoria tramite la join
//SEMBRA FUNZIONARE MA: PERCHÉ? SE IL GESTORE FA LA EXIT SENZA KILL? LA KILL COME SI COMPORTA? POTREI SFRUTTARE IL FLAG?
/*
void killGestoreSegnali() {
    fprintf(stderr, "id gestore segnali: %ld\n", idGestoreSegnali);
    CHECK_NEQ((errno = pthread_kill(idGestoreSegnali, SIGTERM)), 0, "pthread_kill") //sigterm è gestito: aziona terminaCoda e fa exit()
    fprintf(stderr, "La kill e' stata eseguita\n");
    CHECK_NEQ((errno = pthread_join(idGestoreSegnali, NULL)), 0, "join gestore segnali") 
    fprintf(stderr, "La join è stata eseguita\n");
}
*/

//thread che cattura tutti i segnali e li gestisce adeguatamente
void* gestoreSegnali(void* mask){
    int sig;
    while(1){ 
        CHECK_NEQ((errno = sigwait((sigset_t*) mask, &sig)), 0, "sigwait: ")
        //fprintf(stderr, "sblocato\n");
        switch(sig){
            case SIGHUP:
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
                terminaCodaFlag = 1;
                terminaGestoreFlag = 1;
                //fprintf(stderr, "gestore\n");
                pthread_exit(NULL);
            case SIGUSR1:
                stampaFlag = 1;
                break;
            // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket
            //quindi SIGPIPE non fa parte della maschera
            default: ;
        }
    }
    //pthread_exit(NULL); //dovrò killarlo in uscita, serve gestire continuamente i segnali
    //il ciclo è solo per poter ricevere quante volte mi pare SIGUSR1
    //non è un problema se il gestore termina perché agli altri thread i segnali sono mascherati
    //esce con una kill: ricezione segnale SIGTERM
}

//maschera i segnali a tutti i thread e chiama un thread gestore apposta
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
    
    //blocco segnali della maschera a tutti gli altri thread
    CHECK_NEQ((errno = pthread_sigmask(SIG_BLOCK, &mask, NULL)), 0, "pthread_sigmask: ")
    
}
/*
Bloccare è diverso da ignorare. Ignori un segnale installando SIG_IGNcon sigaction().
Dopo che un segnale è stato generato dal kernel o da un processo, il kernel lo rende in attesa di alcuni processi. Si dice che il segnale sia consegnato a un processo una volta che il processo agisce sul segnale. Un processo può bloccare un segnale, che lascia il segnale in sospeso fino a quando non viene sbloccato. Un segnale non bloccato verrà inviato immediatamente. La maschera del segnale specifica quali segnali sono bloccati. Un processo può determinare quali segnali sono in attesa.
La maggior parte degli UNIX non metterà in coda più istanze dello stesso segnale in attesa; solo un'istanza di ciascun segnale può essere in sospeso.
L'impostazione di un'azione del segnale su SIG_IGNper un segnale in attesa causerà l'eliminazione del segnale in attesa, indipendentemente dal fatto che sia bloccato o meno.
E la maschera del segnale di processo contiene l'insieme di segnali che sono attualmente bloccati.
Quando un processo blocca un segnale, un'occorrenza del segnale viene mantenuta fino a quando il segnale non viene sbloccato (i segnali bloccati non vengono persi, mentre i segnali ignorati vengono persi).
*/

void chiudiSocketClient(){
    LOCK(mtxSocket)
    CLOSE(socketClient, "Close socket client: ")
    UNLOCK(mtxSocket)
}

//crea la connessione socket lato client
void creaSocketClient(){
    struct sockaddr_un sa;
    
    CHECK_EQ((socketClient = socket(AF_UNIX, SOCK_STREAM, 0)), -1, "Socket")
    ATEXIT(chiudiSocketClient)

    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    CHECK_EQ(myConnect(socketClient, (struct sockaddr *) &sa, sizeof(sa)), -1, "Connect")
}

void scanDir (char* directory){
	DIR * mydir = NULL;
	//char path_stat[PATHNAME_MAX_DIM] = {"\0"};
    char* path_stat = NULL;
	struct dirent* f = NULL; //contiene #i-node e nome del file
	struct stat buf;
    int sizePath, sizeDir, sizeName;
	
	CHECK_NULL((mydir = opendir(directory)), "Aprendo la directory: ")
	//finché trovo file nella directory e non è attivo il flag di terminazione
	while((errno = 0),(f = readdir(mydir)) != NULL && terminaCodaFlag == 0){
		if(strncmp(f->d_name, ".", 1) != 0 && strncmp(f->d_name, "..", 2) != 0 ){

            sizeName = myStrnlen(f->d_name, PATHNAME_MAX_DIM);
            sizeDir = myStrnlen(directory, PATHNAME_MAX_DIM);
            sizePath = sizeDir + sizeName + 2; //per la barra e lo \0

            if(sizePath < PATHNAME_MAX_DIM){
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
                            //inserimento in bq path_stat
                            CHECK_EQ(push(pool, path_stat), -1, "push")
                            nanosleep(&(pool->delay), NULL);
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

void pushFiles(){
    lPtr temp = NULL;
    while(fileBinari != NULL && terminaCodaFlag == 0){
        push(pool, fileBinari->file);
        nanosleep(&(pool->delay), NULL);
        temp = fileBinari->next;
        free(fileBinari);
        fileBinari = temp;
    }

}