#include "../include/supermercato.h"
#include "../include/cassiere.h"

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

/** Var. GLOBALI */
static int dfd = -1;                 /* file descriptor del socket col direttore */
static int pipefd_sm[2];                /* fd per la pipe, su pipefd_sm[0] lettura, su pipefd_sm[1] scrittura  */

/** LOCK */
static pthread_mutex_t mtx_socket   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_pipe     = PTHREAD_MUTEX_INITIALIZER;

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

static void cleanup(void) {
    if(fcntl(dfd, F_GETFL) >= 0)
        MENO1(close(dfd))

    if(fcntl(pipefd_sm[0], F_GETFL) >= 0)
        MENO1(close(pipefd_sm[0]))

    if(fcntl(pipefd_sm[1], F_GETFL) >= 0)
        MENO1(close(pipefd_sm[1]))
}

int main(int argc, char* argv[]) {
    /* TO DO
     * lettura file config passato da exec
     *
     */
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

    cassa_specific_t *casse_specific;
    EQNULL(casse_specific = calloc(par.K, sizeof(cassa_specific_t)))

    cassa_arg_t *casse;
    EQNULL(casse = calloc(par.K, sizeof(cassa_arg_t)))
    /* per ogni cassiere passo una struttura contenente anche il suo indice */
    for(i = 0; i < par.K; i++) {
        casse[i].tempo_prodotto = par.L;
        casse[i].pool_set = &arg_cas;
        casse[i].cassa_set->index = i;
        casse[i].cassa_set->q = Q[i];
        set_stato_cassa(casse[i].cassa_set, CHIUSA);
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
                            __attribute__((fallthrough));

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
    return 0;
}