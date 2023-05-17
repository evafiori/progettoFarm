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

#define HELP fprintf(stderr, "Usage: %s <fileBinario> [<fileBinario> [<fileBinario>]] [-n <nthread>] [-q <qlen>] [-d <directory-name>] [-t <delay>]\n", argv[0]);
#define NUMBER_OPTION(x, str) trad = isNumber(optarg); if(trad != -1) { if(trad > 0) { x = trad; } else { fprintf(stderr, "Argomento opzione minore o uguale a zero non valido.\n"); exit(EXIT_FAILURE); } } else { perror(str); exit(EXIT_FAILURE); }
#define SETTED_OPTION(c) fprintf(stderr, "Opzione %c già settata\n", c); 

extern int terminaCodaFlag;
extern int terminaGestoreFlag;
extern int stampaFlag; //non mi serve se è direttamente il gestore a scrivere sulla socket

extern pthread_t idGestoreSegnali; 

extern int socketClient;
extern int fdc;

extern pthread_mutex_t mtxSocket;

extern BQueue_t *pool;
extern treeNodePtr_t albero;

lPtr fileBinari = NULL;

extern sigset_t mask;

int pthread_kill(pthread_t thread, int sig); //altrimenti warning

void svuotaFileBinari();

int main(int argc, char * argv[]){
    if(argc <= 2){
        HELP
        return 0;
    }

    pid_t pid;
    
    /*
    char** fileRegolari = NULL;
    int lenFileReg = 0;
    CHECK_NULL((fileRegolari = malloc(sizeof(char*) * (argc-2))), "malloc"); //-2 per togliere nome eseguibile e terminatore NULL
    */
    
    int nWorker = 4, dimCoda = 8, delay = 0;
    char* dir = NULL;
    int setWorker = 0, setCoda = 0, setDelay = 0, setDir = 0;

    int c = 0; 
    int trad = 1;
    opterr = 0; //variabile impostata !=0 x default, se zero getopt non stampa messaggi di errore ma lascia fare al chiamante

    ATEXIT(svuotaFileBinari) //li deallocherà sia per il master che per il collector

    while((c = getopt(argc, argv, ":n:q:d:t:")) != -1 || optind < argc){
        //Per impostazione predefinita, getopt () permuta il contenuto di argv durante la scansione,
        //in modo che alla fine tutte le non opzioni siano alla fine. MA NON LO FA... :( => optind nella condizione del while

        if ( c == -1){
            //non è stato rilevato un argomento opzionale, quindi assumo che l'input sia un file, dovrei fare una coda con i file letti in input
            if(verificaReg(argv[optind])){
                fileBinari = inserisciTesta(fileBinari, argv[optind]);
            }
            else{
                fprintf(stderr, "File %s non regolare.\n", argv[optind]);
            }
            optind++;
            //fprintf(stderr, "%d\n", optind);

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
                        dir = optarg; //controllo idDir
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
    //fprintf(stderr, "Worker: %d\tCoda: %d\tDirectory: %s\tDelay: %d\n", nWorker, dimCoda, dir, delay);
    
    //adesso devo controllare di aver ricevuto qualche file in input:
    //se scannerizzo la directory dopo aver lanciato i thread controllo anche se è stata passata una dir da scan
    if(fileBinari == NULL && setDir == 0){ 
        fprintf(stderr, "Nessun file o directory passati come argomento.\n");
        HELP
        exit(EXIT_FAILURE);
    }

    //verifico e va
    //stampaLista(fileBinari);

    /*
    for(int i = 0; i < lenFileReg; i++){
        printf("%s\n", fileRegolari[i]);
    }
    */
    
    /*
    while(optind < argc){
        printf("%s\n", argv[optind++]);
    }
    */
    
    //prova gestore segnali
    /*
        while (terminaCodaFlag != 1);
    */

    //prova segnali: funziona
    //while(terminaCodaFlag == 0);

    //gestione segnali
    mascheraSegnali();

    CHECK_EQ((pid = fork()), -1, "Fork")
    if(pid == 0){
        //figlio -> collector
        
        creaSocketServer();
        
        ATEXIT(dealloca)

        //provo la socket
        /*
        int msgDim;
        long r = 0;
        char buffer[PATHNAME_MAX_DIM];
        do{
            CHECK_EQ((readn(fdc, &msgDim, sizeof(int))), -1, "readn1")
            if(msgDim > 0){
                CHECK_EQ((readn(fdc, buffer, msgDim)), -1, "readn2")
                CHECK_EQ((readn(fdc, &r, sizeof(long))), -1, "readn3")

                fprintf(stderr, "%d\t%s\t%ld\n", msgDim, buffer, r);
            }
        }while(msgDim >= 0);
        */
        
        leggi();
        
        stampaAlbero(albero);
    }
    else{
        //padre -> masterWoker
        
        //creo thread gestore dei segnali
        CHECK_NEQ((errno = pthread_create(&idGestoreSegnali, NULL, gestoreSegnali, (void*) &mask)), 0, "Thread create: ")
        //IL THREAD È CREATO QUANDO LO DECIDE IL SO
        
        //ATEXIT(killGestoreSegnali)

        //creazione threadpool
        CHECK_EQ(createThreadPool (nWorker, dimCoda, delay), -1, "Creazione threadpool") 

        creaSocketClient();

        if(setDir){
            scanDir(dir);
        }
        pushFiles();
        
        //int i = 1;
        //int msgDim;
        //int w;
        /*
        while(terminaCodaFlag != 1 && i < argc){
            //delay

            LOCK(pool->m)

            //se la coda è piena aspetto
            while(pool->qlen == pool->qsize && terminaCodaFlag != 1){
                WAIT(pool->cfull, pool->m)
            }
            
            if(terminaCodaFlag != 1){
                CHECK_EQ(push(pool, argv[i]), -1, "push")
                //SIGNAL(pool->cempty) //dovrebbe diventare una broadcast?
            }

            SIGNAL(pool->cempty)
            UNLOCK(pool->m)
            
            //
            CHECK_EQ((msgDim = myStrnlen(argv[i], PATHNAME_MAX_DIM)), -1, "myStrnlen")
            msgDim++; //'\0'
            do{
                CHECK_EQ((w = writen(socketClient, &msgDim, sizeof(int))), -1, "writen")
            }while(w == 1);
            do{
                CHECK_EQ((w = writen(socketClient, argv[i], msgDim)), -1, "writen")
            }while(w == 1);
            //manca solo long
            //
            //sleep(1);

            i++;
        }
        */
        LOCK(pool->m)

        terminaCodaFlag = 1;//da mettere se commentato if sopra
        BROADCAST(pool->cempty)

        UNLOCK(pool->m)

        //join prima del messaggio di errore
        for(int i = 0; i < pool->numthreads; i++){
            if (pthread_join(pool->threads[i], NULL) != 0) {
                errno = EFAULT;
                perror("destroyThreadPool");
                //non posso andarmene così, lascerei il collector penzolone
                exit(EXIT_FAILURE);
            }
        }

        //va levato perché lo devono fare i thread
        /*
        msgDim = -1;
        LOCK(mtxSocket)
        CHECK_EQ((w = writen(socketClient, &msgDim, sizeof(int))), -1, "writen termine")
        UNLOCK(mtxSocket)
        */
        
        //prova gestore segnali
        //while (terminaCodaFlag != 1);

        CHECK_EQ((waitpid(pid, NULL, 0)), -1, "waitpid: ")

        if(terminaGestoreFlag == 0){
            CHECK_NEQ((errno = pthread_kill(idGestoreSegnali, SIGTERM)), 0, "pthread_kill") //sigterm è gestito: aziona terminaCoda e fa exit()
            //fprintf(stderr, "La kill e' stata eseguita\n");
        }
        CHECK_NEQ((errno = pthread_join(idGestoreSegnali, NULL)), 0, "join gestore segnali") 
        //fprintf(stderr, "La join è stata eseguita\n");
    }

    return 0;
}

void svuotaFileBinari(){
    if(fileBinari == NULL){
        return ;
    }
    fileBinari = svuotaLista(&fileBinari);
    fileBinari = NULL;
}