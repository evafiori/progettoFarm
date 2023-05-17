
#include <threadpool.h>

//flag dichiarati in master.c
extern int terminaCodaFlag;
extern int stampaFlag;

//struct contenente le informazioni utili ai thread e alla comunicazione tra thread e master, tra cui la più importante: la coda
BQueue_t *pool = NULL;

//in master.c
extern int socketClient;
extern pthread_mutex_t mtxSocket;

//pop: la chiamata deve avvenire in possesso della LOCK
//estrae un elemento dalla coda concorrente
char* pop(BQueue_t *q){
    if(q == NULL){
        errno = EINVAL;
        return NULL;
    }
    char* data = q->queue[q->head];
    q->queue[q->head] = NULL;
    q->head = (q->head+1) % q->qsize;
    q->qlen--;

    return data;
}

//inserisce un elemento in coda concorrente, si occupa dell'acquisizione della lock
//ritorna 0 oppure -1 in caso di errore con errno settato
int push(BQueue_t *q, char* data){
    if (q == NULL || data == NULL) {
        errno = EINVAL;
        return -1;
    }
    LOCK(pool->m)
    //se la coda è piena aspetto
    while(pool->qlen == pool->qsize && terminaCodaFlag != 1){
        WAIT(pool->cfull, pool->m)
    }
    if(terminaCodaFlag != 1){
        q->queue[q->tail] = data;
        q->tail = (q->tail+1) % q->qsize;
        q->qlen++;
    }
    SIGNAL(pool->cempty)
    UNLOCK(pool->m)

    return 0;
}

//calcola la sommatoria richiesta sul file il cui pathname è passato come paramentro
//ritorna il risultato oppure -1 con errno settato
long risultato(char * p){
    FILE * fileInput = NULL;
    long sum = 0, i = 0, x;

    if((fileInput = fopen(p, "rb")) == NULL){
        //errno settato dalla fopen, gestione al chiamante
        fprintf(stderr, "errore apertura file\n");
        return -1;
    }

    while(fread(&x, sizeof(long), 1, fileInput) == 1){ 
        //calcoli
        sum += i*x;
        i++;
    }

    if(ferror(fileInput)){
        //lettura terminata a causa di un errore
        errno = EIO;
        fclose(fileInput);
        return -1;
    }

    fclose(fileInput);
    
    return sum;
}

void *worker_thread(void *threadpool) {    
    BQueue_t *pool = (BQueue_t *)threadpool; // cast
    char* path = NULL;
    long ris;
    int msgDim;
    int w;

    //while(terminaCodaFlag == 0 || pool->qlen > 0) {
    while(1){
        LOCK(pool->m)
        // in attesa di un messaggio o del flag di terminazione
        while(pool->qlen == 0 && terminaCodaFlag == 0) {
            WAIT(pool->cempty, pool->m)
	    }

        if (terminaCodaFlag == 1 && pool->qlen == 0) {
            // devo uscire e non ci sono task in coda
            // l'ultimo thread in vita manda il messaggio di terminazione
            if(pool->threadAttivi == 1){
                msgDim = -1;
                LOCK(mtxSocket)
                CHECK_EQ((w = writen(socketClient, &msgDim, sizeof(int))), -1, "writen termine")
                UNLOCK(mtxSocket)
            }
            pool->threadAttivi--;
            
            SIGNAL(pool->cfull) //nel caso tutto questo fosse stato attivato dal signal handler
            //BROADCAST(pool->cempty) //la fa il master quando vede il flag oppure quando finisce di inserire
            UNLOCK(pool->m)
            return NULL; //pthread exit
        }
        
        //messaggio di stampa
        if(stampaFlag == 1){
            UNLOCK(pool->m)

            msgDim = -2;
            LOCK(mtxSocket)
            CHECK_EQ((w = writen(socketClient, &msgDim, sizeof(int))), -1, "writen termine")
            UNLOCK(mtxSocket)
            stampaFlag = 0;
        }
        else{
            // nuovo task
            CHECK_NULL((path = pop(pool)), "pop")
        
            //faccio la signal al master 
            SIGNAL(pool->cfull)

            UNLOCK(pool->m)

            // eseguo la funzione 
            CHECK_EQ((ris = risultato(path)), -1, "risultato")

            // invio il risultato
            //scrittura sulla socket

            CHECK_EQ((msgDim = (int)myStrnlen(path, PATHNAME_MAX_DIM)), -1, "myStrnlen")
            msgDim++; //'\0'
            
            LOCK(mtxSocket)

            CHECK_EQ((w = writen(socketClient, &msgDim, sizeof(int))), -1, "writen msgDim")
            CHECK_EQ((w = writen(socketClient, path, msgDim)), -1, "writen path")
            CHECK_EQ((w = writen(socketClient, &ris, sizeof(long))), -1, "writen long")
            
            UNLOCK(mtxSocket)
            
            free(path);
            path = NULL;
        }
    }


    UNLOCK(pool->m)

    return NULL;
}

//chiamata in atexit per deallocare la memoria legata al pool dei thread
//libera tutte la risorse dello struct del threadpool
void destroyThreadPool() {    
    if(pool == NULL) {
        errno = EINVAL;
        perror("destryThreadPool");
        return ;
    }

    if(pool->threads) {
        free(pool->threads);
    }

    if(pool->queue != NULL) {
        for(int i=0; i < pool->qsize; i++) {
            if(pool->queue[i] != NULL){
                free(pool->queue[i]);
            } 
        }
        free(pool->queue);
    }
	
    if((errno = pthread_mutex_destroy(&(pool->m))) != 0){
        perror("destroy mutex");        
    }
    if((errno = pthread_cond_destroy(&(pool->cfull))) != 0){
        perror("destroy cond cfull"); 
    }
    if((errno = pthread_cond_destroy(&(pool->cempty))) != 0){
        perror("destroy cond cempty");       
    }

    free(pool);
}

int createThreadPool(int num, int size, int del) {
    //questo controllo io lo faccio già quando leggo gli argomenti
    if(num <= 0 || size < 0) {
	    errno = EINVAL;
        return -1;
    }
    
    CHECK_NULL((pool = (BQueue_t *) malloc(sizeof(BQueue_t))), "malloc pool")
    
    //inizializzazione
    pool->numthreads   = 0;
    pool->threadAttivi = 0;
    pool->qsize = size;
    pool->qlen = 0;
    pool->head = 0;
    pool->tail = 0;

    pool->delay.tv_sec = del / 1000;
    pool->delay.tv_nsec = (del % 1000) * 1000000;

    //allocazione dinamica dell'array dei thread e della coda
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num);
    if (pool->threads == NULL) {
        //error: ENOMEM
        free(pool);
        return -1;
    }
    pool->queue = (char**) malloc(sizeof(char*) * size);
    if (pool->queue == NULL) {
        //error: ENOMEM
        free(pool->threads);
        free(pool);
        return -1;
    }
    
    //inizializzazione
    for(int i = 0; i < pool->qsize; i++){
        pool->queue[i] = NULL;
    }
    
    if ((errno = pthread_mutex_init(&(pool->m), NULL)) != 0){
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return -1;
    }
    if((errno = pthread_cond_init(&(pool->cfull), NULL) != 0)){
        free(pool->threads);
        free(pool->queue);
        free(pool);
        if((errno = pthread_mutex_destroy(&(pool->m))) != 0){
            perror("destroy mutex");
        }
        return -1;
    } 
    if((errno = pthread_cond_init(&(pool->cempty), NULL)) != 0){
        free(pool->threads);
        free(pool->queue);
        free(pool);
        if((errno = pthread_mutex_destroy(&(pool->m))) != 0){
            perror("destroy mutex");
        }
        if((errno = pthread_cond_destroy(&(pool->cfull))) != 0){
            perror("destroy cond cfull");
        }
        return -1;
    }
    
    //creazione thread
    for(int i = 0; i < num; i++) {
        if((errno = pthread_create(&(pool->threads[i]), NULL, worker_thread, (void*)pool)) != 0) {
	    /* errore fatale, libero tutto forzando l'uscita dei threads */
        //la forzo mettendo il flag a più di uno
            terminaCodaFlag = 1;
            destroyThreadPool();
	        errno = EFAULT;
            return -1;
        }
    }
    pool->numthreads = num;
    pool->threadAttivi = num;

    ATEXIT(destroyThreadPool)

    return 0;
}

