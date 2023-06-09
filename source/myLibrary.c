
#include <myLibrary.h>

int nanosleep(const struct timespec *req, struct timespec *rem);

//ritorna il valore intero oppure -1 con errno settato
int isNumber(const char* s) {
    char* e = NULL;
    int val = (int)strtol(s, &e, 0); //converte le stringhe in interi. If this is 0, the base used is determined by the format in the sequence
    if (e != NULL && *e == (char)0) { //e è il puntatore al carattere successivo al numero letto, che quindi sarà il puntatore al terminatore di stringa
        if(errno == ERANGE){
            return -1;
        }
        return val; 
    }
    errno = EINVAL;
    return -1;
}

//ritorna 1 se il file passato è una directory, -1 altrimenti
int verificaDir(char* pathname){
    struct stat info;
    CHECK_EQ(stat(pathname, &info), -1, "stat")
    if (S_ISDIR(info.st_mode)){ 
        return 1;
    }
    else{
        return -1;
    }
}

//ritorna 1 se il file passato è regolare, -1 altrimenti
int verificaReg(char* pathname){
    struct stat info;
    CHECK_EQ(stat(pathname, &info), -1, "stat")
    if (S_ISREG(info.st_mode)){ 
        return 1;
    }
    else{
        return -1;
    }
}


//ritorna la lunghezza se va a buon fine, -1 e setta errno altrimenti
size_t myStrnlen(char* str, size_t max){
    size_t l = strnlen(str, max);
    if(l == max){ //non accetto mai dimensione di 256, massimo 255 e lascio spazio al terminatore di stringa
        errno = ERANGE;
        return -1;
    }
    else{
        return l;
    }
}

//ritorna 1 se è tutto okay e -1 e setta errno altrimenti
//max rappresenta il massimo dei byte finali
int myStrncat(char* s1, char* s2, size_t max){
    int l1, l2;
    l1 = myStrnlen(s1, max);
    l2 = myStrnlen(s2, max);
    if(l1 == -1 || l2 == -1){
        //errno già settato
        return -1;
    }
    if(l1 + l2 < max){ //strettamente minore per il terminatore di stringa
        strncat(s1, s2, l2);
        return 1;
    }
    else{
        errno = ERANGE;
        return -1;
    }
}

//read safe alle interruzioni
/*ritorna 
    -1: in caso di errore con errno settato
    0: EOF
    1: letti tutti i byte richiesti
*/
int readn (int fd, void* buf, size_t n){
	size_t  nleft = n, nread;
	char* bufbuf = (char*)buf;
	while(nleft > 0){
		if((nread = read(fd, bufbuf, nleft)) == -1 && errno != EINTR) {
			return -1;
		}
		if(nread == 0){//EOF
			return 0;
		}
		nleft -= nread;
     	bufbuf += nread;
	}
	return 1;
}

//write safe alle interruzioni
// -1 => errore, errno settato. 
//  0 => okay
int writen (int fd, void* buf, size_t n){
	size_t   nleft = n, nwrite = 0;
	char* bufbuf = (char*)buf;
    //fprintf(stderr, "buf: %s\tbufbuf: %s\n", (char*)buf, bufbuf);
   	while (nleft > 0) {
   		if((nwrite = write(fd, bufbuf, nleft)) == -1 && errno != EINTR) {
	   		//if(nleft == n){
				return -1;
			//}
			//else{
			//	return n - nleft;
			//}
   		}
   		nleft -= nwrite;
   		bufbuf   += nwrite;
   	}
   	return 0; //(n - nleft);
}

//ritorna 0 se la connessione va a buon fine e -1 altrimenti con errno settato
int myConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    struct timespec request;
    request.tv_nsec = 50000;
    request.tv_sec = 0;
    int aspetta = 3; //provo ad eseguire la connect() 3 volte se la connessione viene rifiutata perché nessuno è in ascolto, 
                    //dopo di che assumo che nessuno è in ascolto a causa di un errore sul collector e ritorno -1

    while((errno = 0), (connect(sockfd, addr, addrlen)) < 0){
        /*
        Provo nuovamente la connect se:
            - ENOENT: il socket ancora non esiste
            - EAGAIN: la richiesta non può essere risolta immediatamente
            - EINTR: la chiamata è non bloccante e viene interrotta da un segnale
            - ECONNREFUSED: nessuno è in ascolto 
                -> provo altre volte dopo aver atteso perché richiesta di connessione può arrivare prima che il master si metta in ascolto, altrimenti esco con fallimento
        */
        if(errno == ENOENT || errno == EAGAIN || errno == EINTR || (errno == ECONNREFUSED && (aspetta--) > 0)){ 
            nanosleep(&request, NULL);//aspetto un momento
            //non controllo il valore di ritorno perché non mi interessa se è stata interrotta, mi basta sia passato un po' di tempo
        }
        else{
            return -1;
        }
    }
    return 0;
}