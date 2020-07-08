#include <myutils.h>
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <mypthread.h>
#include <mysocket.h>
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <concurrent_queue.h>
#include <signal.h>
#include <fcntl.h>

#include "../include/pool.h"
#include "../include/protocollo.h"

#define DEBUG

/**********************************************************************************************************
 * LOGICA DEL SUPERMERCATO
 *  - 0) legge parametri di configurazione
 *  - 1) gestione segnali
 *  - MANAGER: è il thread principale ( funzione main() ), gestisce gli altri thread
 *  - genera K thread cassieri
 *      si mettono tutti in attesa su una var. di condizione
 *      - solamente J casse (J <= K) vengono aperte inizialmente
 *  - genera C thrad clienti
 *      entrano tutti subito nel supermercato
 *      al termine vengono riutilizzati fingendosi altri clienti
 *      rientrano nel supermercato quando ne sono usciti E;
 *      la generazione è infinita
 * - comunicazione col Direttore attraverso un SOCKET AF_UNIX, bidirezionale
 * si termina il programma attraverso la ricezione di segnali:
 * - SIGQUIT    terminazione coatta
 * - SIGHUP     terminazione soft
 *********************************************************************************************************/

typedef struct cassiere {
    int index;
    pool_set_t *pool_set;
} cassiere_t;

typedef enum attesa {
    ATTESA = 0,
    SERVITO,
    CHIUSURA_SUPERMERCATO
} attesa_t;

typedef struct queue_elem {
    int num_prodotti;
    attesa_t stato_attesa;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
} queue_elem_t;

/** Var. GLOBALI */
static queue_t **Q;
static int dfd = -1;                 /* file descriptor del direttore */

/** LOCK */
static pthread_mutex_t mtx_socket = PTHREAD_MUTEX_INITIALIZER;

static void *cassiere(void *arg);
static void *notificatore(void *arg);
static void *cliente(void *arg);

/*****************************************************
 * SIGNAL HANDLER SINCRONO
 * - aspetta la ricezione di segnali specificati da una maschera
 * - una volta ricevuto un segnale esegue l'azione da svolgere
 *****************************************************/
static void *sync_signal_handler(void *arg) {
    sigset_t *mask = (sigset_t *) arg;

    int sig_captured,           /* conterrà il segnale cattuarato dalla sigwait */
        err;

    for(;;) {
        if(get_stato_supermercato() != APERTO)
            break;
        PTH(err, sigwait(mask, &sig_captured))
        switch(sig_captured) {
            case SIGINT:
            case SIGQUIT: /* chiusura immediata */
                set_stato_supermercato(CHIUSURA_IMMEDIATA);
                break;
            case SIGHUP:
                set_stato_supermercato(CHIUSURA_SOFT);
                break;
            default: ;
        }
    }
    return (void *) NULL;
}

void cleanup() {
    if(fcntl(dfd, F_GETFL) >= 0)
        MENO1(close(dfd))
}

int main() {
    /* Variabili di supporto */
    int i,                      /* indice dei cicli */
        err;                    /* conterrà i valori di ritorno di alcune chiamate (es: pthread) */

    /*************************************************************
    * Lettura parametri di configurazione
    *************************************************************/
    param_t par;
    MENO1(get_params_from_file(&par))

    /***************************************************************************************
    * Gestione segnali
     * - thread signal handler, gestirà SIGQUIT e SIGHUP
     *      maschero tutti i segnali nel thread main
     *          => tutti i thread creati da questo thread erediteranno la signal mask
    ****************************************************************************************/
    sigset_t mask;
    MENO1(sigemptyset(&mask))
    MENO1(sigaddset(&mask, SIGINT))
    MENO1(sigaddset(&mask, SIGQUIT))
    MENO1(sigaddset(&mask, SIGHUP))

    PTH(err, pthread_sigmask(SIG_BLOCK, &mask, NULL))
    /*
     * thread signal handler
     */
    pthread_t tid_tsh;
    void *status_tsh;
    PTH(err, pthread_create(&tid_tsh, NULL, sync_signal_handler, (void *) &mask))

    /*************************************************************
    * Generazione cassieri
     * - inizializzazione code
    *  - thread pool di K cassieri
    *  - attivo solamente J casse
    *************************************************************/
    EQNULL(Q = calloc(par.K, sizeof(queue_t *)))
    for(i = 0; i < par.K; i++) {
        start_queue(&(Q[i]));
    }

    pthread_t *tid_casse;       /* tid dei cassieri*/
    EQNULL(tid_casse = calloc(par.K, sizeof(pthread_t)))
    void **status_casse;        /* conterrà i valori di ritorno dei thread cassieri */
    EQNULL(status_casse = calloc(par.K, sizeof(void *)))

    /*
     * argomenti del cassiere
     */
    pool_set_t arg;     /* conterrà i parametri per far attesa e lavoro del cassiere */
    pool_start(&arg);
    arg.jobs = par.J;

    cassiere_t *casse;
    EQNULL(casse = calloc(par.K, sizeof(cassiere_t)))
    /*
     * per ogni cassiere passo una struttura contenente il suo indice
     */
    for(i = 0; i < par.K; i++) {
        casse[i].pool_set = &arg;
        casse[i].index = i;
    }
    for(i = 0; i < par.K; i++) {
        // PTH(err, pthread_create((tid_casse + i), NULL, cassiere, (void *) (casse+i) ))
    }

    /*************************************************************
    * Generazione Clienti
    *  - thread pool di C clienti
    *************************************************************/
    pthread_t *tid_clienti;       /* tid dei clienti*/
    EQNULL(tid_clienti = calloc(par.C, sizeof(pthread_t)))
    void **status_clienti;        /* conterrà i valori di ritorno dei thread clienti */
    EQNULL(status_clienti = calloc(par.C, sizeof(void *)))

    /*
     * argomenti del cliente
     */
    pool_set_t arg_cl;     /* conterrà i parametri per far attesa e lavoro del cassiere */
    pool_start(&arg_cl);
    arg_cl.jobs = par.C;

    for(i = 0; i < par.K; i++) {
       // PTH(err, pthread_create((tid_casse + i), NULL, cliente, (void *) &arg_cl ))
    }


    /*************************************************************
    * Comunicazione col Direttore
    * - crea un socket di connessione
    * - si connette tramite l'indirizzo del server
     * Come prima cosa invia il suo PID, successivamente si mette in ascolto
    *************************************************************/
    SOCKETAF_UNIX(dfd)

    atexit(cleanup);

    SOCKETAF_UNIX(dfd)
    struct sockaddr_un serv_addr;
    SOCKADDR_UN(serv_addr, SOCKET_SERVER_NAME)

    while(connect(dfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        if(errno == ENOENT) { // se il server non è ancora stato creato
            sleep(1);
        } else {
            perror("connect");
            exit(EXIT_FAILURE);
        }
    }
#ifdef DEBUG
    printf("[CLIENT] connessione accettata!\n");
#endif
    /*
     * scritture del supermercato sul socket:
     *      sono concorrenti, in quanto sia clienti che
     *      cassieri vogliono scriverci
     *      mentre il manager (main) è colui che riceve i messaggi
     */
    msg_code_t type_msg;        /* conterrà il tipo del messaggio inviato/ricevuto */

    /*
     * invio del PID
     */
    type_msg = MANAGER_PID;
    MENO1(writen(dfd, &type_msg, sizeof(msg_code_t)))

    pid_t smpid = getpid();
    MENO1(writen(dfd, &smpid, sizeof(pid_t)))

    /*************************************************************
   * Attessa tramite POLL su
     *  .smfd
     *  .pipefd[0]
   *************************************************************/

    /*************************************************************
    * Terminazione: cleanup
    *************************************************************/
    /*
     * signal handler
     */
    PTH(err, pthread_join(tid_tsh, &status_tsh))
    if(status_tsh == PTHREAD_CANCELED) {
        printf("Thread signal handler cancellato");
    }
    else if(status_tsh != (void *) 0) {
        perror("Thread signal handler exited");
        exit(EXIT_FAILURE);
    }
    /*
     * cassieri
     */
    pool_destroy(&arg);
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_join(tid_casse[i], status_casse+i))
        if(status_casse[i] == PTHREAD_CANCELED) {
            printf("Thread cassiere cancellato");
        }
        else if( status_casse[i] != (void *) 0) {
            perror("Thread cassiere exited");
            exit(EXIT_FAILURE);
        }
    }

    free(tid_casse);
    free(status_casse);
    PTH(err, pthread_cond_destroy(&(arg.cond)))
    PTH(err, pthread_mutex_destroy(&(arg.mtx)))

}

/**********************************************************************************************************
 * LOGICA DEL CASSIERE
 * - viene creato => aspetta che la cassa venga aperta
 * - la prima votla che viene aperta spawna un thread secondario
 * - attiva il thread notificatore associato
 * - attende che ci siano clienti
 * - se ci sono clienti in coda li serve
 *
 * - se è CHIUSA, il manager la può svegliare per:
 *          comunicazione dal direttore -> apre
 *          chiusura supermercato       -> terminazione thread
 * - se è APERTA
 *          se riceve dal direttore di chiudere -> va in attesa, va nello stato CHIUSA
 *          se CHIUSURA IMMEDIATA
 *              -> serve cliente corrente
 *                      avverte tutti i clienti in coda
*                       termina
 *          se CHIUSURA SOFT
 *              -> serve tutti i clienti in coda, quando la coda è vuota terminerà
 *
 **********************************************************************************************************/
static void *cassiere(void *arg) {
    int err;
    cassiere_t *C = (cassiere_t *) arg;
    pool_set_t *P = C->pool_set;
    
    pthread_t tid_notificatore = -1;
    void *status_notificatore;

    while(get_stato_supermercato() == APERTO) {
        PTH(err, pthread_mutex_lock(&(P->mtx)))
        while(P->jobs == 0){
            PTH(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
            PTH(err, pthread_mutex_unlock(&(P->mtx)))
            if(get_stato_supermercato() != APERTO) {
                return (void *) NULL;
            }
            PTH(err, pthread_mutex_lock(&(P->mtx)))
        }
        P->jobs--;
        PTH(err, pthread_mutex_unlock(&(P->mtx)))
        
        if(tid_notificatore == -1) {
            PTH(err, pthread_create(&tid_notificatore, NULL, notificatore, (void *) NULL))
        }


    }
    /*
     * terminazione
     */
    PTH(err, pthread_join(tid_notificatore, &status_notificatore))

    return (void *) 0;
}

static void *notificatore(void *arg) {
    return (void *) NULL;
}

static void *cliente(void *arg) {
    pool_set_t *P = (pool_set_t *) arg;
    int p = 1,
        t,
        err;

    while(get_stato_supermercato() == APERTO) {
        while(P->jobs == 0) {
            PTH(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
            /*
             * viene svegliato: si deve controllare se si deve terminare;
             * lascia la mutex per non avere due risorse lockate
             */
            PTH(err, pthread_mutex_unlock(&(P->mtx)))
            if(get_stato_supermercato() != APERTO) {
                pthread_exit((void *) NULL);
            }
            PTH(err, pthread_mutex_lock(&(P->mtx)))
        }

        /*
         * fa acquisti
         */
        if(p == 0) {
            /*
             * chiede permesso al direttore per uscire
             */
        } else {
            /*
            * si mette in fila in una cassa
            */
            queue_elem_t elem;
            elem.num_prodotti = p;
        }
    }

    return (void *)NULL;
}