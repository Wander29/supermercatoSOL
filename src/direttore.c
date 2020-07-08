#include <myutils.h>
#include <protocollo.h>
#include <mypthread.h>
#include <mysocket.h>
#include <mypoll.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <signal.h>
#include <fcntl.h>
#include "../include/protocollo.h"

#define DEBUG

/** var. globali */
static int pipefd[2];           /* pipe di comunicazione fra signal handler e main */
static int listen_ssfd = -1;    /* listen server socket fd */
static int smfd = -1;           /* supermercato fd*/

// struct sockaddr_un listen_server_addr; definita in protocollo.h (è l'indirizzo del server)

/*****************************************************
 * SIGNAL HANDLER SINCRONO
 * - aspetta la ricezione di segnali specificati da una maschera
 * - una volta ricevuto un segnale scrive sulla pipe il segnale ricevuto
 *****************************************************/
static void *sync_signal_handler(void *arg) {
    sigset_t *mask = (sigset_t *) arg;

    int sig_captured,           /* conterrà il segnale cattuarato dalla sigwait */
    err;

    for(;;) {
        if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
            break;

        PTH(err, sigwait(mask, &sig_captured))
        switch(sig_captured) {
            case SIGINT:
            case SIGQUIT:
                sig_captured = SIGQUIT;    /* utile per gestire più segnali come SIGQUIT (ad es. SIGINT) */
            case SIGHUP:
                MENO1(writen(pipefd[1], &sig_captured,sizeof(int)))
                break;
            default: ;
        }
    }
    return (void *) NULL;
}

static void inline usage(char *str) {
    printf("Usage: %s [-<OPTION>]\nOPTION:\n-s\tavvia il processo supermercato\n-h\thelp\n", str);
}

static void cleanup() {
    MENO1(unlink(SOCKET_SERVER_NAME))
    /*
     * provo ad ottenere i flag del fd: se li ottengo vuol dire che il fd è attivo,
     *      pertanto provvedo a chiuderlo
     */
    if(fcntl(listen_ssfd, F_GETFL) >= 0)
        MENO1(close(listen_ssfd))

    if(fcntl(smfd, F_GETFL) >= 0)
        MENO1(close(smfd))

    if(fcntl(pipefd[0], F_GETFL) >= 0)
        MENO1(close(pipefd[0]))

    if(fcntl(pipefd[1], F_GETFL) >= 0)
        MENO1(close(pipefd[1]))
}

/***********************************************************************************************
 * LOGICA DEL DIRETTORE (del supermercato)
 * - se viene passata l'opzione -s forka il supermercato
 *      altrimenti il supermercato verrà avviato separatamente e questo comunicherà
 *      il suo PID al direttore
 * - gestione segnali
 *      thread signal handler che scrive sulla pipe per avvisare
 * - crea un socket di comunicazione col supermercato e si mette in ascolto su:
 *         . socket di comunicazione
 *         . pipe[0]
 ************************************************************************************************/
int main(int argc, char *argv[]) {

    /***************************************************************************************
     * Controllo parametri di ingresso
    ****************************************************************************************/
    if (argc > 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    pid_t pid_sm = -1;          /* pid del supermercato */

    if (argc == 2) {
        if (argv[1][0] == '-' && argv[1][2] == '\0') {
            switch (argv[1][1]) {
                case 's':
                    pid_sm = fork();
                    switch (pid_sm) {
                        case 0:     /* figlio */
                            execl(PATH_TO_SUPERMARKET, "supermercato", (char *) NULL);
                        case -1:    /* errore */
                            perror("fork");
                            exit(EXIT_FAILURE);
                            break;
                        default:    /* padre */
#ifdef DEBUG
                            printf("[DIRETTORE] Supermercato aperto!\n");
#endif
                            break;
                    }
                    break;
                case 'h':
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                    break;
                default:
                    printf("Opzione non valida\n");
                    exit(EXIT_FAILURE);
            }
        } else {
            printf("Opzione non valida\n");
            exit(EXIT_FAILURE);
        }
    }

    /** Variaibili di supporto */
    int err,
            i;
    /***************************************************************************************
    * Gestione segnali
     * - inizializza la pipe di comunicazione fra signal handler e main
     * - thread signal handler, gestirà SIGQUIT e SIGHUP
     *      maschero tutti i segnali nel thread main
    ****************************************************************************************/
    MENO1(pipe(pipefd))

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

    /***************************************************************************************
     * Comunicazione col supermercato: creazione socket di comunicazione
     * - SOCKET AF_UNIX per l'instaurazione della connessione
     * - creazione SOCKET
     * - socket si mette in ascolto
     * - attende la richiesta di connessione del supermercato
     * - una volta ottenuto il nuovo socket di comunicazione
     *      chiude il socket iniziale
    ****************************************************************************************/
    SOCKETAF_UNIX(listen_ssfd)  /* creo il socket AF_UNIX */

    atexit(cleanup);            /* elimina l'indirizzo del server e il socket all'uscita */

    SOCKETAF_UNIX(listen_ssfd)
    struct sockaddr_un serv_addr;
    SOCKADDR_UN(serv_addr, SOCKET_SERVER_NAME)

    MENO1(bind(listen_ssfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
    MENO1(listen(listen_ssfd, MAX_BACKLOG))

    /***************************************************************************************
     * attendo UNA richiesta di connessione dal supermercato
     *      - inizializzo le strutture per la POLL
     *      - uso la POLL per attendere una connessione in un certo intervallo
     *              e monitorare eventuali comunicazione del signal handler
     ***************************************************************************************/
        struct pollfd *pollfd_v;    /* array di pollfd */
        EQNULL(pollfd_v = start_pollfd())
        int polled_fd = 0;          /* conterrà il numero di fd che si stanno monitorando */
        struct pollfd tmp;          /* pollfd di supporto per gli inserimenti */

        tmp.fd = listen_ssfd;
        tmp.events = POLLIN;        /* attende eventi di lettura non con alta priorità */
        MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

        tmp.fd = pipefd[0];
        tmp.events = POLLIN;
        MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

    /***************************************************************************************
     * MULTIPLEXING, tramite POLL:
     *  attendo I/O su più file descriptors
     *      .smfd
     *      .pipe[0]
     * Inizialmente attende UNA unica richiesta di connessione del supermercato
     *
     *
     *  Se non si disponde del PID del supermercato, la prima cosa che si fa è richiederlo
     *  In caso di chiusura immediata il direttore rimanda il segnale al supermercato
     *  e aspetta la sua terminazione
     *  In caso di chiusura soft il direttore rimane in attesa di notifiche da parte
     *  dei cassieri, e può quindi continuare a decidere di aprire o chiudere casse
     *  fino alla terminazione vera e propria.
     *  In questo caso il direttore verrà informato dal supermercato che ha terminato i clienti
     *  da servire e quindi termina.
    ****************************************************************************************/
#ifdef DEBUG
    printf("[DIRETTORE] In attesa di una connessione\n");
#endif

    for(;;) {
        if (get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
#ifdef DEBUG
            printf("[DIRETTORE] sto per terminare!\n");
#endif
            break;
        }
        /*
        * MULTIPLEXING: attendo I/O su vari fd
        */
        if(poll(pollfd_v, polled_fd, -1) == -1) {       /* aspetta senza timeout, si blocca */
            if(errno == EINTR && get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                /*
                 * in caso di interruzione per segnali non gestiti dall'handler
                 */
                printf("[SERVER] chiusura server\n");
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

        for(i=0; i < current_pollfd_array_size; i++) {
            if (pollfd_v[i].revents & POLLIN) {      /* fd pronto per la lettura! */
                int fd_curr = pollfd_v[i].fd;

                if(fd_curr == pipefd[0]) {
                    /*
                     * il signal handler ha ricevuto un segnale
                     */
                    int sig_captured;
                    MENO1(readn(pipefd[0], &sig_captured, sizeof(int)))
                    /*
                     * manda il segnale ricevuto al supermercato
                     */
#ifdef DEBUG
                    printf("[DIRETTORE] ricevuto segnale [%d]!\n", sig_captured);
#endif
                    set_stato_supermercato(CHIUSURA_IMMEDIATA);
                    // MENO1(kill(pid, sig_captured))
                }
                else if(fd_curr == listen_ssfd) {
                    /*
                     * accetto la connessione del supermercato, dopodichè chiudo il socket
                     */
                    MENO1(smfd = accept(listen_ssfd, NULL, 0))  /* socket di comunicazione col supermercato */
                    tmp.fd = smfd;
                    tmp.events = POLLIN;
                    MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

                    MENO1(pollfd_remove(pollfd_v, i, &polled_fd))
                    current_pollfd_array_size--;
                    MENO1(close(listen_ssfd))
                }
                else {
                    /*
                     * ricevuta comunicazione dal supermercato
                     */
                    msg_code_t type_msg;
                    MENO1(readn(smfd, &type_msg, sizeof(msg_code_t)))

                    switch(type_msg){
                        case MANAGER_PID: {
                            MENO1(readn(smfd, &pid_sm, sizeof(pid_t)))
                            break;
                        default: ;
                        }
                    }
                }
            }
        }
    }

    /***************************************************************************************
     * Terminazione DIRETTORE
    ****************************************************************************************/

    PTH(err, pthread_cancel(tid_tsh))
    PTH(err, pthread_join(tid_tsh, &status_tsh))
    if( status_tsh == PTHREAD_CANCELED ) {
#ifdef DEBUG
        printf("Signal Handler: cancellato\n");
#endif
    } else {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    return 0;
}
