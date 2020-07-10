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
#include <time.h>

#include "../include/pool.h"
#include "../include/protocollo.h"
#include "../include/concurrent_queue.h"

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

#define CASSA_APERTA_CHECK(Q)                               \
    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {    \
        set_stato_cassa((Q), IN_TERMINAZIONE);              \
        return 1;                                           \
    }                                                       \
    if(get_stato_cassa((Q)) == CHIUSA) {                    \
        return 1;                                           \
    }

#define MAX_TEMPO_FISSO 80
#define MIN_TEMPO_FISSO 20

typedef struct cliente_arg {
    /* comune a tutti i clienti */
    pool_set_t *pool_set;

    /* specifico per ogni cliente */
    int index;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    int permesso_uscita;
} cliente_arg_t;

typedef enum stato_cassa {
    CHIUSA = 0,     /* IF(cassa.stato == CHIUSA && get_stato_supermerato() == CHIUS.IMM) => cliente esci */
    APERTA,
    IN_TERMINAZIONE
} stato_cassa_t;

typedef struct cassa_arg {
    /* comune a tutte le casse */
    pool_set_t *pool_set;

    /* specifico per ogni cassa */
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    queue_t *q;
    stato_cassa_t stato;
    int index;
} cassa_arg_t;

typedef enum attesa {
    ATTESA = 0,
    SERVITO
} attesa_t;

typedef struct queue_elem {
    int num_prodotti;
    attesa_t stato_attesa;
    pthread_cond_t cond_cl_q;
    pthread_mutex_t mtx_cl_q;
} queue_elem_t;

typedef enum pipe_msg_code {
    CLIENTE_RICHIESTA_PERMESSO = 0,
    CLIENTI_TERMINATI,
    SIG_CHIUSURA_IMM_RICEVUTO = 900
} pipe_msg_code_t;

/** Var. GLOBALI */
static queue_t **Q;
static int dfd = -1;                 /* file descriptor del socket col direttore */
static int pipefd_sm[2];                /* fd per la pipe, su pipefd_sm[0] lettura, su pipefd_sm[1] scrittura  */

/** LOCK */
static pthread_mutex_t mtx_socket   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_pipe     = PTHREAD_MUTEX_INITIALIZER;

static inline int pipe_write(pipe_msg_code_t *type, int *param) {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_pipe))

    if(writen(pipefd_sm[1], type, sizeof(pipe_msg_code_t)) == -1) {
        perror("writen");
        return -1;
    }
    if(param != NULL) {
        if(writen(pipefd_sm[1], param, sizeof(int)) == -1) {
            perror("writen");
            return -1;
        }
    }

    PTH(err, pthread_mutex_unlock(&mtx_pipe))

    return 0;
}

static inline int socket_write(sock_msg_code_t *type, int *param){
    int err;

    PTH(err, pthread_mutex_lock(&mtx_socket))

    if(writen(dfd, type, sizeof(sock_msg_code_t)) == -1) {
        perror("writen");
        return -1;
    }
    if(param != NULL) {
        if(writen(dfd, param, sizeof(int)) == -1) {
            perror("writen");
            return -1;
        }
#ifdef DEBUG_SOCKET
        printf("[SUPERMERCATO] HO scritto sul socket [%d, %d]\n", *type, *param);
#endif
    } else {
#ifdef DEBUG_SOCKET
        printf("[SUPERMERCATO] HO scritto sul socket [%d]\n", *type);
#endif
    }

    PTH(err, pthread_mutex_unlock(&mtx_socket))

    return 0;
}

static void *cassiere(void *arg);
static void *notificatore(void *arg);
static int cassiere_attesa_lavoro(pool_set_t *P);
queue_elem_t *cassiere_pop_cliente(queue_t *Q);
static stato_cassa_t get_stato_cassa(cassa_arg_t *cassa);
static void set_stato_cassa(cassa_arg_t *cassa, const stato_cassa_t s);

static void *cliente(void *arg);
static void cliente_attendi_permesso_uscita(cliente_arg_t *t);
static void consenti_uscita_cliente(cliente_arg_t *t);


/*****************************************************
 * SIGNAL HANDLER SINCRONO
 * - aspetta la ricezione di segnali specificati da una maschera
 * - una volta ricevuto un segnale esegue l'azione da svolgere
 *****************************************************/
static void *sync_signal_handler(void *useless) {
    int err;
    sigset_t mask;
    MENO1(sigemptyset(&mask))
    MENO1(sigaddset(&mask, SIGQUIT))
    MENO1(sigaddset(&mask, SIGHUP))
    MENO1(sigaddset(&mask, SIGUSR1))

    PTH(err, pthread_sigmask(SIG_SETMASK, &mask, NULL))
    int sig_captured;
    pipe_msg_code_t msg = SIG_CHIUSURA_IMM_RICEVUTO;

    for(;;) {
        PTH(err, sigwait(&mask, &sig_captured))
        switch(sig_captured) {
            case SIGQUIT: /* chiusura immediata */
                puts("SIGQUIT");
                set_stato_supermercato(CHIUSURA_IMMEDIATA);
                MENO1(writen(pipefd_sm[1], &msg, sizeof(pipe_msg_code_t)))
                break;
            case SIGHUP:
                puts("SIGHUP");
                set_stato_supermercato(CHIUSURA_SOFT);
                // MENO1(writen(pipefd_sm[1], &msg, sizeof(pipe_msg_code_t)))
                break;
            default: /* SIGUSR1*/
                puts("SIGUSR1");
                return (void *) 0;
        }
    }
    return (void *) NULL;
}

void cleanup() {
    if(fcntl(dfd, F_GETFL) >= 0)
        MENO1(close(dfd))

    if(fcntl(pipefd_sm[0], F_GETFL) >= 0)
        MENO1(close(pipefd_sm[0]))

    if(fcntl(pipefd_sm[1], F_GETFL) >= 0)
        MENO1(close(pipefd_sm[1]))
}

int main() {
#ifdef GDB
    sleep(10);
#endif
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
    MENO1(sigfillset(&mask))
    PTH(err, pthread_sigmask(SIG_SETMASK, &mask, NULL))

    /*
     * thread signal handler
     * - inizializzo PIPE di comunicazione
     */
    MENO1(pipe(pipefd_sm))

    pthread_t tid_tsh;
    void *status_tsh;
    PTH(err, pthread_create(&tid_tsh, NULL, sync_signal_handler, (void *) NULL))

    /*************************************************************
    * Comunicazione col Direttore
    * - crea un socket di connessione
    * - si connette tramite l'indirizzo del server
    *************************************************************/
    SOCKETAF_UNIX(dfd)

    atexit(cleanup);

    struct sockaddr_un serv_addr;
    SOCKADDR_UN(serv_addr, SOCKET_SERVER_NAME)

    int num_tentativi = 0;
    while(connect(dfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1 && (num_tentativi++ < 5)) {
        if(errno == ENOENT) { // se il server non è ancora stato creato
            sleep(1);
        } else {
            perror("connect");
            exit(EXIT_FAILURE);
        }
    }

#ifdef DEBUG_MANAGER
    printf("[MANAGER] connessione col Direttore accettata!\n");
#endif

    /*
     * scritture del supermercato sul socket:
     *      sono concorrenti, in quanto sia clienti che
     *      cassieri vogliono scriverci
     *      mentre il manager (main) è colui che riceve i messaggi
     */
    /*************************************************************************************************
    * MULTIPLEXING, tramite POLL:
    *  attendo I/O su più file descriptors
    *      .dfd
    *      .pipe[0]
     *
     * Sulla pipe può scrivere:
     *      - signal handler        dopo aver ricevuto un segnale,
     *                                  avverte di controllare lo stato del supermercato
     *      - clienti               per richiedere al direttore il permesso di uscita:
     *                                  sarà il manager ad inoltrare la richiesta, ricevere la risposta
     *                                  e a far uscire il cliente
   *************************************************************************************************/

    struct pollfd *pollfd_v;    /* array di pollfd */
    EQNULL(pollfd_v = start_pollfd())
    int polled_fd = 0;          /* conterrà il numero di fd che si stanno monitorando */
    struct pollfd tmp;          /* pollfd di supporto per gli inserimenti */

    tmp.fd = dfd;
    tmp.events = POLLIN;        /* attende eventi di lettura non con alta priorità */
    MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

    tmp.fd = pipefd_sm[0];
    tmp.events = POLLIN;
    MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

    pipe_msg_code_t type_msg_pipe;
    sock_msg_code_t type_msg_sock;
    int param;

    /*************************************************************
   * Generazione cassieri
    * - inizializzazione code
   *  - thread pool di K cassieri
   *  - attivo solamente J casse inizialmente
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
    pool_set_t arg_cas;     /* conterrà i parametri per far attesa e lavoro del cassiere */
    pool_start(&arg_cas);
    arg_cas.jobs = par.J;   /* casse attive inizialmente */

    cassa_arg_t *casse;
    EQNULL(casse = calloc(par.K, sizeof(cassa_arg_t)))
    /* per ogni cassiere passo una struttura contenente il suo indice */
    for(i = 0; i < par.K; i++) {
        casse[i].pool_set = &arg_cas;
        casse[i].index = i;
        casse[i].q = Q[i];
        set_stato_cassa(casse + i, CHIUSA)
    }
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_create((tid_casse + i), NULL, cassiere, (void *) (casse+i) ))
    }

    /*************************************************************
   * Generazione Client
    * - inizializzazione pipe di comunicazione
   *  - thread pool di C clienti
   *************************************************************/

    pthread_t *tid_clienti;       /* tid dei clienti*/
    EQNULL(tid_clienti = calloc(par.C, sizeof(pthread_t)))
    void **status_clienti;        /* conterrà i valori di ritorno dei thread clienti */
    EQNULL(status_clienti = calloc(par.C, sizeof(void *)))

    /*
     * argomenti del cliente
     */
    pool_set_t arg_cl;
    MENO1(pool_start(&arg_cl))
    arg_cl.jobs = par.C;

    cliente_arg_t *clienti;
    EQNULL(clienti = calloc(par.C, sizeof(cliente_arg_t)))
    /*
     * per ogni cliente passo una struttura contenente il suo indice
     */

    for(i = 0; i < par.C; i++) {
        clienti[i].pool_set = &arg_cl;
        clienti[i].index = i;
        clienti[i].permesso_uscita = 0;
    }
    for(i = 0; i < par.C; i++) {
        PTH(err, pthread_create((tid_clienti + i), NULL, cliente, (void *) (clienti + i) ))
    }
#ifdef DEBUG_MANAGER
    printf("[MANAGER] clienti creati\n");
#endif

    for(;;) {
        if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
            break;
        /*
       * MULTIPLEXING: attendo I/O su vari fd
       */
#ifdef DEBUG_MANAGER
        printf("[MANAGER] POLL\n");
#endif
        if(poll(pollfd_v, polled_fd, -1) == -1) {       /* aspetta senza timeout, si blocca */
            if(errno == EINTR && get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                /*
                 * in caso di interruzione per segnali non gestiti dall'handler
                 */
                printf("[SUPERMERCATO] chiusura supermercato\n");
                break;
            } else {
                perror("accept");
                exit(EXIT_FAILURE);
            }
        }
        /* il cnt di fd monitorati può cambiare durante il ciclo
         * => è necessario mantenerlo consistente durante una scansione dell'array
         * in caso di chiusure => va decrementato*/
        int current_pollfd_array_size = polled_fd;

        for(i = 0; i < current_pollfd_array_size; i++) {
            if (pollfd_v[i].revents & POLLIN) {      /* fd pronto per la lettura! */
                int fd_curr = pollfd_v[i].fd;

                if (fd_curr == pipefd_sm[0]) {
                    MENO1(readn(pipefd_sm[0], &type_msg_pipe, sizeof(pipe_msg_code_t)))
#ifdef DEBUG_MANAGER
                    printf("[MANAGER] PIPE [%d]!\n", type_msg_pipe);
#endif
                    switch (type_msg_pipe) {
                        case CLIENTE_RICHIESTA_PERMESSO:
                            MENO1(readn(pipefd_sm[0], &param, sizeof(int)))
                            type_msg_sock = CLIENTE_SENZA_ACQUISTI;
#ifdef DEBUG_MANAGER
                            printf("[MANAGER] Ricevuta richiesta di uscire del cliente [%d]\n", param);
#endif
                            MENO1(socket_write(&type_msg_sock, &param))
                            break;
                        case CLIENTI_TERMINATI:
#ifdef DEBUG_MANAGER
                            printf("[MANAGER] Chisura Soft && Clienti terminati! Via alle danze!\n");
#endif
                            set_stato_supermercato(CHIUSURA_IMMEDIATA);
                        case SIG_CHIUSURA_IMM_RICEVUTO:
#ifdef DEBUG_MANAGER
                            printf("[MANAGER] Tocca chiudeee\n");
#endif
                            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                                type_msg_sock = MANAGER_IN_CHIUSURA;
                                MENO1(socket_write(&type_msg_sock, NULL))
                                goto terminazione_supermercato;
                            }
                            break;
                        default:;
                    }
                }
                else if(fd_curr == dfd) {
                    MENO1(readn(dfd, &type_msg_sock, sizeof(sock_msg_code_t)))
#ifdef DEBUG_MANAGER
                    printf("[MANAGER] SOCKET [%d]!\n", type_msg_sock);
#endif
                    switch(type_msg_sock) {
                        case DIRETTORE_PERMESSO_USCITA:
                            MENO1(readn(dfd, &param, sizeof(int)))
#ifdef DEBUG_MANAGER
                            printf("[MANAGER] Ricevuto PERMESSO di uscire per il cliente [%d]\n", param);
#endif
                            consenti_uscita_cliente(clienti + param);
                            break;
                        default: ;
                    }
                }
            }
        }
    }

    /*************************************************************
    * Terminazione: cleanup
    *************************************************************/
terminazione_supermercato:
#ifdef DEBUG
    printf("[SUPERMERCATO] terminazione iniziata\n");
    fflush(stdout);
#endif
    /*
     * cassieri
     * sveglio tutti gli eventuali cassieri dormienti, in attesa di lavoro o in attesa sulla coda
     */
    /*
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_cond_signal(&(casse[i].pool_set->cond)))
    }
     */
    PTH(err, pthread_cond_broadcast(&(arg_cas.cond)))

    pool_destroy(&arg_cas);


    for(i = 0; i < par.K; i++) {
        free_queue(Q[i], NO_DYNAMIC_ELEMS);
        PTH(err, pthread_join(tid_casse[i], status_casse+i))
        PTHJOIN(status_casse[i], "Cassiere")
    }
    free(tid_casse);
    free(status_casse);
    free(casse);

    /*
     * clienti
     */
    pool_destroy(&arg_cl);

    for(i = 0; i < par.C; i++) {
        PTH(err, pthread_join(tid_clienti[i], status_clienti+i))
        PTHJOIN(status_clienti[i], "Cliente")
    }
    free(tid_clienti);
    free(status_clienti);
    free(clienti);
    /*
     * poll
     */
    pollfd_destroy(pollfd_v);

    /*
     * signal handler
     */
    PTH(err, pthread_kill(tid_tsh, SIGUSR1))
    PTH(err, pthread_join(tid_tsh, &status_tsh))
    PTHJOIN(status_tsh, "Signal Handler")

#ifdef DEBUG
    fprintf(stderr, "[SUPERMERCATO] CHIUSURA CORRETTA\n");
#endif
    fflush(stdout);
    return 0;
}

/**********************************************************************************************************
 * LOGICA DEL CASSIERE
 * - viene creato => aspetta che la cassa venga aperta
 * - la prima votla che viene aperta spawna un thread secondario notificatore
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
    cassa_arg_t *C = (cassa_arg_t *) arg;
    pool_set_t *P = C->pool_set;

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))
    
    pthread_t tid_notificatore = -1;
    void *status_notificatore;
    queue_elem_t *cliente;
    int i = C->index;

    /** parametri del Cassiere */
    unsigned seedp = 0;
    int tempo_fisso = (rand_r(&seedp) % (MAX_TEMPO_FISSO - MIN_TEMPO_FISSO + 1)) + MIN_TEMPO_FISSO; // 20 - 80 ms

    struct timespec ts = {0, tempo_fisso};
    MENO1(nanosleep(&ts, NULL))

    for(;;) {
        /* attende il lavoro */
        if( (err = cassiere_attesa_lavoro(P)) == 1) // termina
            goto terminazione_cassiere;
        else if(err < 0)
            fprintf(stderr, "Errore durante l'attesa di un lavoro per la cassa [%d]\n", i);

        /* ha ottenuto il lavoro
         *  - se è la prima apertura, avvia il thread notificatore associato
         *  - apre la cassa e si mette in attesa di clienti */
        if(tid_notificatore == (pthread_t ) -1) {
            PTH(err, pthread_create(&tid_notificatore, NULL, notificatore, (void *) NULL))
        }

        set_stato_cassa(C, APERTA);

        /********************************************************************************
         * GESTIONE CLIENTI(cassa APERTA)
         * - controlla che la cassa non sia stata chiusa || che il supermercato non
         *          stia chiudendo immediatamente
         * - se ci sono clienti, ne serve uno
         * - aggiorna il numero di clienti in coda
         *      SE il supermercato sta chiudendo (NON immediatamente) e il numero di clienti
         *      in attesa di essere serviti nel supermercato è 0;
         *      invia al Manager tale comunicazione
         ********************************************************************************/
        for(;;) {
            if( (cliente = cassiere_pop_cliente()) == 1 ) {
                stato_cassa_t stato = get_stato_cassa(C->q);
                if(CHIUSA == stato) {
                    // avverte i clienti di cambiare cassa

                    break;
                } else if(IN_TERMINAZIONE == stato) {
                    // avvisa i clienti di terminare
                    break;
                }
            }
            /*
             * cliente estratto, lo servo
             */
            // cassiere_servi_cliente();

            /*
             * una volta servito il clienti decremento il contatore di clienti in coda
             */
            if(dec_num_clienti_in_coda() == 0 && get_stato_supermercato() == CHIUSURA_SOFT) {
                pipe_msg_code_t msg = CLIENTI_TERMINATI;
                pipe_write(&msg, NULL);
            }
        }
    }

terminazione_cassiere:
    if(tid_notificatore != -1) {
        PTH(err, pthread_join(tid_notificatore, &status_notificatore))
        PTHJOIN(status_notificatore, "Notificatore cassiere")
    }

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))


    return (void *) 0;
}

static stato_cassa_t get_stato_cassa(cassa_arg_t *cassa) {
    int err;
    stato_cassa_t stato;

    PTH(err, pthread_mutex_lock(&(cassa->mtx))) {
        stato = cassa->stato;
    } PTH(err, pthread_mutex_unlock(&(cassa->mtx)))

    return stato;
}

static void set_stato_cassa(cassa_arg_t *cassa, const stato_cassa_t s) {
    int err;

    PTH(err, pthread_mutex_lock(&(cassa->mtx))) {
        cassa->stato = s;
    } PTH(err, pthread_mutex_unlock(&(cassa->mtx)))

}

static int cassiere_attesa_lavoro(pool_set_t *P) {
    int err;

    PTH(err, pthread_mutex_lock(&(P->mtx))) {
        while(P->jobs == 0){
            PTH(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
            /*
             * SE la cassa era chiusa, e viene svegliata dal Manager
             * per la chiusura del supermercato => termina
             */
            PTH(err, pthread_mutex_unlock(&(P->mtx)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                return 1;
            }
            PTH(err, pthread_mutex_lock(&(P->mtx)))
        }
        P->jobs--;
    } PTH(err, pthread_mutex_unlock(&(P->mtx)))

    return 0;
}

static void *notificatore(void *arg) {
#ifdef DEBUG_CLIENTE
    printf("[NOTIFICATORE] sono nato!\n");
#endif
    return (void *) NULL;
}

queue_elem_t *cassiere_pop_cliente(queue_t *Q) {
    CASSA_APERTA_CHECK(Q)
    int err;          // retval, per errori
    queue_elem_t *val;

    PTH(err, pthread_mutex_lock(&(Q->mtx))) {
        while(Q->nelems == 0) {
            PTH(err, pthread_cond_wait(&(Q->cond_read), &(Q->mtx)))
            PTH(err, pthread_mutex_unlock(&(Q->mtx)))

            CASSA_APERTA_CHECK(Q)
        }
        val = (queue_elem_t *) get_from_queue(Q);
        if(val == NULL)
            fprintf(stderr, "NO elements in Queue: %s\n", __func__);

    } PTH(err, pthread_mutex_unlock(&(Q->mtx)))

    return val;
}

static void *cliente(void *arg) {
    cliente_arg_t *C = (cliente_arg_t *) arg;
    pool_set_t *P = C->pool_set;

#ifdef DEBUG_CLIENTE
    printf("[CLIENTE %d] sono nato!\n", C->index);
#endif

    int p = 0,
        t,
        err;

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    pipe_msg_code_t type_msg;

    // while(get_stato_supermercato() == APERTO) {
        PTH(err, pthread_mutex_lock(&(P->mtx)))
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
        P->jobs--;
        C->permesso_uscita = 0;
        PTH(err, pthread_mutex_unlock(&(P->mtx)))
        /*
         * fa acquisti
         */
        if(p == 0) {
            /*
             * vuole comunicare al direttore di voler uscire:
             *      scrive sulla pipe in mutua esclusione la sua richiesta, completa di indice cliente
             *      e si mette in attesa di una risposta
             */
#ifdef DEBUG_CLIENTE
            printf("[CLIENTE %d] fateme uscìììììì!\n", C->index);
#endif
            type_msg = CLIENTE_RICHIESTA_PERMESSO;
            MENO1(pipe_write(&type_msg, &(C->index)))
            cliente_attendi_permesso_uscita(C);
        } else {
            /*
            * si mette in fila in una cassa
            */
            queue_elem_t elem;
            elem.num_prodotti = p;
        }
    //}

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))

    pthread_exit((void *)NULL);
}

static void cliente_attendi_permesso_uscita(cliente_arg_t *t) {
    int err;

    PTH(err, pthread_mutex_lock(&(t->mtx))) {
        while(t->permesso_uscita == 0)
            PTH(err, pthread_cond_wait(&(t->cond), &(t->mtx)))

#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Permesso di uscita ricevuto!\n", t->index);
#endif
    } PTH(err, pthread_mutex_unlock(&(t->mtx)))
}

static void consenti_uscita_cliente(cliente_arg_t *t) {
    int err;

    PTH(err, pthread_mutex_lock(&(t->mtx))) {
        t->permesso_uscita = 1;
        PTH(err, pthread_cond_signal(&(t->cond)))
    } PTH(err, pthread_mutex_unlock(&(t->mtx)))
}

int cliente_push(queue_t *Q, queue_elem_t *new_elem) {
    int r;          // retval, per errori

    PTH(r, pthread_mutex_lock(&(Q->mtx))) {
        if(insert_into_queue(Q, (void *) new_elem) == -1) {
            PTH(r, pthread_mutex_unlock(&(Q->mtx)))
            // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
            return -1;
        }
        PTH(r, pthread_mutex_unlock(&(Q->mtx)))
        inc_num_clienti_in_coda();
        PTH(r, pthread_cond_signal(&(Q->cond_read)))
    } PTH(r, pthread_mutex_unlock(&(Q->mtx)))

    return 0;
}