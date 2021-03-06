#include <supermercato.h>
#include <parser_writer.h>
#include <mypoll.h>
#include <protocollo.h>
#include <pool.h>
#include <cassiere.h>
#include <cliente.h>

#include <signal.h>
#include <fcntl.h>

/**********************************************************************************************************
 * LOGICA DEL SUPERMERCATO
 *  - egge parametri di configurazione
 *  - gestione segnali
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
int dfd = -1;                /* file descriptor del socket col direttore, accessibile dal cleanup */
int pipefd_sm[2];            /* fd per la pipe, su pipefd_sm[0] lettura, su pipefd_sm[1] scrittura  */
min_queue_t min_queue = { NULL, -1 };   /* conterrà le informazioni sulla coda
                                                    * apparentemente più conveniente in cui inserirsi */
stato_sm_t stato_supermercato = APERTO;

/** LOCK */
pthread_mutex_t mtx_stato_supermercato  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_socket              = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_pipe                = PTHREAD_MUTEX_INITIALIZER;
pthread_spinlock_t spin;

/*****************************************************
 * SIGNAL HANDLER SINCRONO
 * - aspetta la ricezione di segnali specificati da una maschera: SIGQUIT, SIGHUP, SIGUSR1
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
    pipe_msg_code_t msg = SIG_RICEVUTO;

    for(;;) {
        PTH(err, sigwait(&mask, &sig_captured))
        switch(sig_captured) {
            case SIGQUIT: /* chiusura immediata */
                MENO1LIB(pipe_write(&msg, 1, &sig_captured), ((void *)-1))
                break;
            case SIGHUP:
                MENO1LIB(pipe_write(&msg, 1, &sig_captured), ((void *)-1))
                break;
            default: /* SIGUSR1*/
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
    /***************************************************************************************
     * Controllo parametri di ingresso
    ****************************************************************************************/
    if (argc != 2) {
        printf("Usage: %s non va eseguito direttamente. È necessario che venga forkato " \
               "dal processo direttore, il quale deve passare come argomento alla fork" \
               "il path al file di configurazione\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Variabili di supporto */
    int i,                      /* indice dei cicli */
        err;                    /* conterrà i valori di ritorno di alcune chiamate (es: pthread) */

    /*************************************************************
    * Lettura parametri di configurazione
    *************************************************************/
    param_t par;
    MENO1LIB(get_params_from_file(&par, argv[1]), -1)

    /***************************************************************************************
    * Gestione segnali
     * - thread signal handler, gestirà SIGQUIT e SIGHUP
     *      maschero tutti i segnali nel thread main
     *          => tutti i thread creati da questo thread erediteranno la signal mask
    ****************************************************************************************/
    struct sigaction sa;
    MENO1(sigaction(SIGPIPE, NULL, &sa))
    sa.sa_handler = SIG_IGN;
    MENO1(sigaction(SIGPIPE, &sa, NULL))

    sigset_t mask;
    MENO1(sigfillset(&mask))
    MENO1(sigdelset(&mask, SIGPIPE))
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

    atexit(cleanup);            /* all'uscita eliminerà anche il socket */

    struct sockaddr_un serv_addr;
    SOCKADDR_UN(serv_addr, SOCKET_SERVER_NAME)

    int num_tentativi = 0;      /* tento la connessione al massimo 5 volte, aspettando quindi 5 secondi al massimo */
    while(connect(dfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        if(num_tentativi++ > 5 || errno != ENOENT) {
            perror("connect");
            exit(EXIT_FAILURE);
        } else {
            sleep(1);
        }
    }
    /*
     * scritture del supermercato sul socket e pipe:
     *      sono concorrenti, in quanto sia clienti che
     *      cassieri vogliono scriverci
     *      mentre il manager (main) è colui che riceve i messaggi, è l'unico a leggere
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
     *                              per avvisare della propria entrata/uscita
   *************************************************************************************************/
    struct pollfd *pollfd_v;    /* array di pollfd */
    EQNULL(pollfd_v = start_pollfd())
    int polled_fd = 0;          /* conterrà il numero di fd che si stanno monitorando */
    struct pollfd tmp;          /* pollfd di supporto per gli inserimenti */

    tmp.fd = dfd;
    tmp.events = POLLIN;        /* attende eventi di lettura non con alta priorità */
    MENO1LIB(pollfd_add(&pollfd_v, tmp, &polled_fd), -1)

    tmp.fd = pipefd_sm[0];
    tmp.events = POLLIN;
    MENO1LIB(pollfd_add(&pollfd_v, tmp, &polled_fd), -1)

    pipe_msg_code_t type_msg_pipe;
    sock_msg_code_t type_msg_sock;
    int param;

    /*************************************************************
   * Generazione cassieri
    * - dichiarazione delle code (inizializzate e distrutte nel main)
   *  - thread pool di K cassieri
   *  - attivo solamente J casse inizialmente
   *************************************************************/
    PTH(err, pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE))

    pthread_t *tid_casse;       /* tid dei cassieri*/
    EQNULL(tid_casse = calloc(par.K, sizeof(pthread_t)))
    void **status_casse;        /* conterrà i valori di ritorno dei thread cassieri */
    EQNULL(status_casse = calloc(par.K, sizeof(void *)))

    /***********************************************************
     * Argomenti passati ai thread CASSIERI
     ***********************************************************/
    /** Argomenti per il LOG */
    log_set_t log_set;
    log_set.C                       = par.C;
    log_set.K                       = par.K;
    EQNULL(log_set.log_casse = calloc(par.K, sizeof(cassa_log_t)))
    EQNULL(log_set.log_clienti = calloc(par.C, sizeof(queue_t *)))

    /** Code per le casse */
    queue_t **Q;
    EQNULL(Q = calloc(par.K, sizeof(queue_t *)))
    for(i = 0; i < par.K; i++) {
        EQNULL(log_set.log_casse[i].aperture        = start_queue())
        EQNULL(log_set.log_casse[i].clienti_serviti = start_queue())
        EQNULL(Q[i]                                 = start_queue())
    }

    /** argomenti del POOL */
    pool_set_t arg_cas;
    MENO1LIB(pool_start(&arg_cas), -1)
    arg_cas.jobs = par.J;   /* casse attive inizialmente */

    /** argomenti COMUNI a tutte le casse */
    cassa_com_arg_t com_casse;
    com_casse.pool_set        = &arg_cas;
    com_casse.tempo_prodotto  = par.L;
    com_casse.A               = par.A;

    /** argomenti SPECIFICI, riempiti nel ciclo */
    cassa_public_t *casse_public;
    EQNULL(casse_public = calloc(par.K, sizeof(cassa_public_t)))

    cassa_arg_t *casse;
    EQNULL(casse = calloc(par.K, sizeof(cassa_arg_t)))

    for(i = 0; i < par.K; i++) {
        /* comuni */
        casse[i].shared = &com_casse;

        /* specifici */
        casse[i].log_cas = log_set.log_casse + i;
        PTH(err, pthread_mutex_init(&(casse_public[i].mtx_cassa), NULL))
        PTH(err, pthread_cond_init(&(casse_public[i].cond_queue), NULL))
        PTH(err, pthread_cond_init(&(casse[i].cond_notif), NULL))
        casse_public[i].index = i;
        casse_public[i].q     = Q[i];
        casse_public[i].stato = CHIUSA;

        casse[i].cassa_set = casse_public + i;
    }
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_create((tid_casse + i), NULL, cassiere, (void *) (casse + i) ))
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

    /***********************************************************
     * Argomenti passati ai thread CLIENTI
     ***********************************************************/
    /** argomenti del POOL */
    pool_set_t arg_cl;
    MENO1LIB(pool_start(&arg_cl), -1)
    arg_cl.jobs = par.C;

    /** argomenti COMUNI */
    client_com_arg_t com_cl;
    com_cl.numero_casse = par.K;
    com_cl.casse = casse_public;
    com_cl.pool_set = &arg_cl;
    com_cl.T = par.T;
    com_cl.P = par.P;
    com_cl.S = par.S;
    com_cl.current_last_id_cl = 0;

    PTH(err, pthread_mutex_init(&(com_cl.mtx_id_cl), NULL))
    PTH(err, pthread_cond_init(&(com_cl.cond_id_cl), NULL))

    /** argomenti SPECIFICI, riempiti nel ciclo */
    cliente_arg_t *clienti;
    EQNULL(clienti = calloc(par.C, sizeof(cliente_arg_t)))

    for(i = 0; i < par.C; i++) {
        /* LOG */
        EQNULL(log_set.log_clienti[i] = start_queue())
        clienti[i].log_cl = log_set.log_clienti[i];

        clienti[i].shared = &com_cl;
        clienti[i].index = i;
        clienti[i].permesso_uscita = 0;

        PTH(err, pthread_mutex_init(&(clienti[i].mtx), NULL))
        PTH(err, pthread_cond_init(&(clienti[i].cond), NULL))

        EQNULL(clienti[i].elem = calloc(1, sizeof(queue_elem_t)))
        PTH(err, pthread_cond_init(&(clienti[i].elem->cond_cl_q), NULL))
        PTH(err, pthread_mutex_init(&(clienti[i].elem->mtx_cl_q), NULL))
    }
    for(i = 0; i < par.C; i++) {
        PTH(err, pthread_create((tid_clienti + i), NULL, cliente, (void *) (clienti + i) ))
    }

    int cnt_clienti_attivi = 0;
    int cnt_usciti = 0;
    for(;;) {
        if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
            break;
        /*
       * MULTIPLEXING: attendo I/O su vari fd
       */
        if(poll(pollfd_v, polled_fd, -1) == -1) {       /* aspetta senza timeout, si blocca */
            if(errno == EINTR && get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                /*
                 * in caso di interruzione per segnali non gestiti dall'handler
                 */
                printf("[SUPERMERCATO] chiusura supermercato\n");
                break;
            } else {
                perror("poll");
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

                if (fd_curr == pipefd_sm[0]) { /* messaggio sulla PIPE */
                    MENO1(readn(pipefd_sm[0], &type_msg_pipe, sizeof(pipe_msg_code_t)))
                    switch (type_msg_pipe) {

                        case CLIENTE_RICHIESTA_PERMESSO:
                            MENO1(readn(pipefd_sm[0], &param, sizeof(int)))
                            type_msg_sock = CLIENTE_SENZA_ACQUISTI;
                            MENO1(socket_write(&type_msg_sock, 1, &param))
                            break;

                        case CLIENTE_ENTRATA: // è necessario ai fini del progetto
                            cnt_clienti_attivi++;
                            break;

                        case CLIENTE_USCITA:
                            cnt_clienti_attivi--;
                            cnt_usciti++;
                            if(get_stato_supermercato() != APERTO && cnt_clienti_attivi == 0) {
                                set_stato_supermercato(CHIUSURA_IMMEDIATA);
                                type_msg_sock = MANAGER_IN_CHIUSURA;
                                MENO1(socket_write(&type_msg_sock, 0))
                                goto terminazione_supermercato;
                            }
                            if(cnt_usciti == par.E) {
                                /*
                                 * fornisco E lavori, faccio una broadcast ma solamente E clienti lavoreranno
                                 */
                                NOTZERO(ch_jobs(&arg_cl, par.E))
                                PTH(err, pthread_cond_broadcast(&(arg_cl.cond)))
                                cnt_usciti = 0;
                            }
                            break;

                        case SIG_RICEVUTO:
                            MENO1(readn(pipefd_sm[0], &param, sizeof(int)))
                            if(param == SIGHUP) {
                                set_stato_supermercato(CHIUSURA_SOFT);
                                PTH(err, pthread_cond_broadcast(&(arg_cl.cond)))
                            }
                            else if(param == SIGQUIT) {
                                set_stato_supermercato(CHIUSURA_IMMEDIATA);
                                type_msg_sock = MANAGER_IN_CHIUSURA;
                                MENO1(socket_write(&type_msg_sock, 0))
                                goto terminazione_supermercato;
                            }
                            break;

                        default:;
                    }
                }
                else if(fd_curr == dfd) { /* Socket di comunicazione col direttore */
                    /*
                     * messaggio dal Direttore
                     */
                    MENO1(readn(dfd, &type_msg_sock, sizeof(sock_msg_code_t)))
                    switch(type_msg_sock) {
                        case DIRETTORE_PERMESSO_USCITA:
                            MENO1(readn(dfd, &param, sizeof(int)))
                            NOTZERO(set_permesso_uscita(clienti+param, 1))
                            PTH(err, pthread_cond_signal(&(clienti[param].cond)))

                            break;

                        case DIRETTORE_APERTURA_CASSA:
                            set_jobs(&arg_cas, 1);
                            PTH(err, pthread_cond_signal(&(arg_cas.cond)))
                            break;

                        case DIRETTORE_CHIUSURA_CASSA:
                            MENO1(readn(dfd, &param, sizeof(int)))

                            NOTZERO(set_stato_cassa(casse_public + param, CHIUSA))
                            PTH(err, pthread_cond_signal(&(casse_public[param].cond_queue)))
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

    /***********************************************************
     * THREAD IN ATTESA DI LAVORO
     *      clienti e cassieri dormienti in attesa di lavoro
     *      sulle rispettive JOBS vengono svegliati
     ***********************************************************/
    PTH(err, pthread_cond_broadcast(&(arg_cas.cond)))
    PTH(err, pthread_cond_broadcast(&(arg_cl.cond)))

    /****************************************************************
     * TERMINAZIONE CASSIERI
     * sveglio tutti gli eventuali cassieri dormienti sulla cond della QUEUE
     ****************************************************************/
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_cond_signal(&(casse_public[i].cond_queue)))
        PTH(err, pthread_join(tid_casse[i], status_casse+i))
        PTHJOIN(status_casse[i], "Cassiere")
    }
    free(tid_casse);
    free(status_casse);

    /****************************************************************
     * TERMINAZIONE CLIENTI
     *      i Clienti in coda sono avvisati dai rispettivi cassieri
     *      i Clienti che ancora non si erano messi in coda escono da soli
     *      i Clienti in attesa del permesso di uscita vengono svegliati
     ****************************************************************/
    for(i = 0; i < par.C; i++) {
        PTH(err, pthread_cond_signal(&(clienti[i].cond)))
        PTH(err, pthread_join(tid_clienti[i], status_clienti+i))
        PTHJOIN(status_clienti[i], "Cliente")
    }
    free(tid_clienti);
    free(status_clienti);

    /********************************************************************
     * Distruzione degli argomenti passati ai thread
     *      - distruggo anche le mutex e le cond
     ********************************************************************/
     /* Cassieri
      *     code e mutex/cond di ogni cassiere */
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_cond_destroy(&(casse_public[i].cond_queue)))
        PTH(err, pthread_mutex_destroy(&(casse_public[i].mtx_cassa)))
        PTH(err, pthread_cond_destroy(&(casse[i].cond_notif)))
        if(free_queue(Q[i], NO_DYNAMIC_ELEMS) != 0)
            printf("[CODA %d] Errore di terminazione\n", i);
    }
    free(Q);

    MENO1LIB(pool_destroy(&arg_cas), -1)
    free(casse_public);
    free(casse);

    /* Clienti
     * - mutex/cond comune
     * - mutex/cond di ogni thread cliente */
    PTH(err, pthread_mutex_destroy(&(com_cl.mtx_id_cl)))
    PTH(err, pthread_cond_destroy(&(com_cl.cond_id_cl)))
    for(i = 0; i < par.C; i++) {
        PTH(err, pthread_mutex_destroy( &(clienti[i].mtx)))
        PTH(err, pthread_cond_destroy(  &(clienti[i].cond)))
        PTH(err, pthread_cond_destroy(  &(clienti[i].elem->cond_cl_q)))
        PTH(err, pthread_mutex_destroy( &(clienti[i].elem->mtx_cl_q)))
        free(clienti[i].elem);
    }

    MENO1LIB(pool_destroy(&arg_cl), -1)
    free(clienti);

    MENO1LIB(write_log(par.Z, &log_set), -1)

    /* LOG :    sono liberati all'interno di write_log al momento
     *          dell'estrazione delle informazioni dalle strutture */
    free(log_set.log_clienti);
    free(log_set.log_casse);

    /* signal handler */
    PTH(err, pthread_kill(tid_tsh, SIGUSR1))
    PTH(err, pthread_join(tid_tsh, &status_tsh))
    PTHJOIN(status_tsh, "Signal Handler")

    /* poll */
    pollfd_destroy(pollfd_v);

    /* mutex globali */
    PTH(err, pthread_spin_destroy(&spin))
    PTH(err, pthread_mutex_destroy(&mtx_socket))
    PTH(err, pthread_mutex_destroy(&mtx_pipe))
    PTH(err, pthread_mutex_destroy(&mtx_stato_supermercato))

    return 0;
}