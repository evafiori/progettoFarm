#define _POSIX_C_SOURCE 200809

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <myLibrary.h>
#include <master.h>
#include <threadpool.h>
#include <collector.h>
#include <tree.h>

//macro utilizzate durante la scansione con getopt
//messaggio di usage
#define HELP fprintf(stderr, "Usage: %s <fileBinario> [<fileBinario> [<fileBinario>]] [-n <nthread>] [-q <qlen>] [-d <directory-name>] [-t <delay>]\n", argv[0]);
//traduzione di un argomento numerico
#define NUMBER_OPTION(x, str) trad = isNumber(optarg); if(trad != -1) { if(trad > 0) { x = trad; } else { fprintf(stderr, "Argomento opzione minore o uguale a zero non valido.\n"); exit(EXIT_FAILURE); } } else { perror(str); exit(EXIT_FAILURE); }
//messaggio di errore per opzione già settata
#define SETTED_OPTION(c) fprintf(stderr, "Opzione %c già settata\n", c); 

//flag dichiarati in master.c
extern int terminaCodaFlag;
extern int terminaGestoreFlag;
extern int stampaFlag;

//in master.c
extern sigset_t mask;
extern pthread_t idGestoreSegnali; 
extern int socketClient;
extern int fdc;
extern pthread_mutex_t mtxSocket;

//in threadpool.c
extern BQueue_t *pool;

//albero binario dichiarato in tree.c
extern treeNodePtr_t albero;

//lista dei file binari letti in argv
lPtr fileBinari = NULL;

int pthread_kill(pthread_t thread, int sig); //altrimenti warning

void svuotaFileBinari();

int main(int argc, char * argv[]){
    if(argc <= 2){
        HELP
        return 0;
    }

    pid_t pid;
    
    int nWorker = 4, dimCoda = 8, delay = 0;
    char* dir = NULL;
    int setWorker = 0, setCoda = 0, setDelay = 0, setDir = 0;

    int c = 0; 
    int trad = 1;
    opterr = 0; //variabile impostata !=0 x default, se zero getopt non stampa messaggi di errore ma lascia fare al chiamante

    ATEXIT(svuotaFileBinari) //li deallocherà sia per il master che per il collector

    while((c = getopt(argc, argv, ":n:q:d:t:")) != -1 || optind < argc){
        //Per impostazione predefinita, getopt () permuta il contenuto di argv durante la scansione,
        //in modo che tutte le non opzioni siano alla fine. MA NON LO FA... => optind nella condizione del while

        if ( c == -1){
            //non è stato rilevato un argomento opzionale, quindi assumo che l'input sia un file, dovrei fare una coda con i file letti in input
            if(verificaReg(argv[optind])){
                fileBinari = inserisciTesta(fileBinari, argv[optind]);
            }
            else{
                fprintf(stderr, "File %s non regolare.\n", argv[optind]);
            }
            optind++;
            
        }
        else{
            switch(c){
                case 'n': //numero dei thread worker
                    if(!setWorker){
                        NUMBER_OPTION(nWorker, "-n: argomento non valido\n")
                        setWorker = 1;
                    }
                    else{
                        SETTED_OPTION('n')
                        HELP
                        exit(EXIT_FAILURE);
                    }
                    break;
                case 'q': //lunghezza della coda concorrente
                    if(!setCoda){
                        NUMBER_OPTION(dimCoda, "-q: argomento non valido\n")
                        setCoda = 1;
                    }
                    else{
                        SETTED_OPTION('q')
                        HELP
                        exit(EXIT_FAILURE);
                    }
                    break;
                case 'd': //directory contenti file binari
                    if(!setDir){
                        dir = optarg; 
                        //controllo che sia effettivamente una directory valida
                        if(!verificaDir(dir)){
                            fprintf(stderr, "Directory specificata non valida.\n");
                            dir = NULL;
                        }
                        else{
                            setDir = 1;
                        }
                    }
                    else{
                        SETTED_OPTION('d')
                        HELP
                        exit(EXIT_FAILURE);
                    }
                    break;
                case 't':  //delay tra richieste successive ai thread worker
                    if(!setDelay){
                        NUMBER_OPTION(delay, "-t: argomento non valido\n")
                        setDelay = 1;
                    }
                    else{
                        SETTED_OPTION('t')
                        HELP
                        exit(EXIT_FAILURE);
                    }
                    break;
                case '?':
                    fprintf(stderr, "-%c: opzione non valida.\n", (char)c);
                    HELP
            }
        }
    }
    
    //adesso devo controllare di aver ricevuto qualche file in input oppure una directory da scannerizzare
    if(fileBinari == NULL && setDir == 0){ 
        fprintf(stderr, "Nessun file o directory passati come argomento.\n");
        HELP
        exit(EXIT_FAILURE);
    }

    //gestione segnali
    mascheraSegnali();

    CHECK_EQ((pid = fork()), -1, "Fork")
    if(pid == 0){
        //figlio -> collector
        
        //creazione socket e attesa connessione
        creaSocketServer();
        
        //deallocazione albero binario
        ATEXIT(dealloca)

        //lettura dalla socket
        leggi();
        
        //stampa albero binario
        stampaAlbero(albero);
    }
    else{
        //padre -> masterWoker
        
        //creo thread gestore dei segnali
        CHECK_NEQ((errno = pthread_create(&idGestoreSegnali, NULL, gestoreSegnali, (void*) &mask)), 0, "Thread create: ")
        
        //creazione threadpool
        CHECK_EQ(createThreadPool (nWorker, dimCoda, delay), -1, "Creazione threadpool") 

        //creazione socket e connessione
        creaSocketClient();

        //se passata una directory: scansione file 
        if(setDir){
            scanDir(dir);
        }

        //metto in coda concorrente i file passati come argomenti
        pushFiles();
        
        //ottengo la lock dopo aver messo in coda tutti i task
        LOCK(pool->m)

        //avvio protocollo di terminazione e sveglio eventuali thread in attesa
        terminaCodaFlag = 1;
        BROADCAST(pool->cempty)

        UNLOCK(pool->m)

        //join dei thread
        for(int i = 0; i < pool->numthreads; i++){
            if (pthread_join(pool->threads[i], NULL) != 0) {
                errno = EFAULT;
                perror("destroyThreadPool");
                exit(EXIT_FAILURE);
            }
        }

        CHECK_EQ((waitpid(pid, NULL, 0)), -1, "waitpid: ")

        //termino e attendo anche il gestore dei segnali
        if(terminaGestoreFlag == 0){
            CHECK_NEQ((errno = pthread_kill(idGestoreSegnali, SIGTERM)), 0, "pthread_kill") //sigterm è gestito: aziona terminaCoda e fa exit()
        }
        CHECK_NEQ((errno = pthread_join(idGestoreSegnali, NULL)), 0, "join gestore segnali") 
    }

    return 0;
}

//svuota la lista dei file binari ricevuti come argomenti se ancora presenti in lista al momento della terminazione
void svuotaFileBinari(){
    if(fileBinari == NULL){
        return ;
    }
    fileBinari = svuotaLista(&fileBinari);
    fileBinari = NULL;
}