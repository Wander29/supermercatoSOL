#include <myutils.h>
#include <protocollo.h>
#include <mypthread.h>
#include <mysocket.h>
#include <mypoll.h>
#include <parser_writer.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "../include/protocollo.h"
#include "../include/parser_writer.h"
#include "../lib/lib-include/mypoll.h"
#include "../lib/lib-include/myutils.h"

/** var. globali */
static int pipefd_dir[2];       /* pipe di comunicazione fra signal handler e main */
static int listen_ssfd = -1;    /* listen server socket fd */
static int smfd = -1;           /* supermercato fd*/

/*****************************************************
 * SIGNAL HANDLER SINCRONO
 * - aspetta la ricezione di segnali specificati da una maschera
 * - una volta ricevuto un segnale scrive sulla pipe il segnale ricevuto
 *****************************************************/
static void *sync_signal_handler(void *useless) {
    int err;
    sigset_t mask;
    MENO1(sigemptyset(&mask))
    MENO1(sigaddset(&mask, SIGINT))
    MENO1(sigaddset(&mask, SIGQUIT))
    MENO1(sigaddset(&mask, SIGHUP))
    MENO1(sigaddset(&mask, SIGUSR1))

    PTH(err, pthread_sigmask(SIG_SETMASK, &mask, NULL))

    int sig_captured;           /* conterrà il segnale cattuarato dalla sigwait */

    for(;;) {
        PTH(err, sigwait(&mask, &sig_captured))
        switch(sig_captured) {
            // case SIGINT:
                // sig_captured = SIGQUIT;    /* utile per gestire più segnali come SIGQUIT (ad es. SIGINT) */
                   // __attribute__((fallthrough));
            case SIGQUIT:
            case SIGHUP:
                MENO1(writen(pipefd_dir[1], &sig_captured, sizeof(int)))
                break;
            default: /* SIGUSR1 */
                return (void *) 0;
        }
    }

    return (void *) 0;
}

inline static void usage(char *str) {
    printf("Usage: %s <OPTION>\nOPTION:\n-h\thelp\n\n"
           "-c <FILE>\tpassa FILE come file di configurazione per il programma.\n"  \
           "FILE deve essere del tipo:\n\n"                                         \
           "#sono un commento (hashtag ad inizio riga);  verrò ignorato \n"         \
           "#numero clienti, con C=0 non ci sono clienti\n"                         \
           "C = x;\t\t[>=0]\n"                                                         \
           "#[...] indica il vincolo che il parametro deve rispettare, NON deve essere presente nel file\n"            \
           "#numero casse, con K=0 non si sono casse\n"                             \
           "K = x;\t\t[>=0]\n"                                                          \
           "#soglia sopra la quale rientrano i clienti, con E=0 non rientrano\n"    \
           "E = x;\t\t[ IN [0, C] ]\n"                                                  \
           "#tempo massimo di acquisti [ms]\n"                                      \
           "T = x;\t\t[>10]\n"                                                        \
           "#numero massimo prodotti acquistabili, con P=0 escono tutti senza acquisti\n"\
           "P = x;\t\t[>=0]\n"                                                             \
           "#ampiezza [ms] intervallo di ricerca nuova cassa dei clienti in coda\n"\
           "S = x;\t\t[>=10]\n"                                                              \
           "#tempo di gestione di un prodotto da parte di ogni cassiere\n"          \
           "L = x;\t\t[>=0]\n"                                                               \
           "#numero casse aperte inizialmente\n"                                    \
           "J = x;\t\t[ IN [0, K] ]\n"                                                               \
           "#ampiezza [ms] intervallo di comunicazione tra cassiere e direttore\n" \
           "A = x;\t\t[>=0]\n"                                                             \
           "#soglia chiusura casse\n"                                               \
           "S1 = x;\t\t[>=1]\n"                                                              \
           "#soglia apertura casse\n"                                               \
           "S2 = x;\t\t[>=1]\n"                                                     \
           "#nome file di LOG, estensione csv, se esiste lo sovrascrive\n"          \
"Z = str.csv\n"                                                                                \
           "NON è necessario che i parametri siano in questo ordine, si devono rispettare le seguenti dipendenze:\n" \
           "prima C poi E;\tprima K poi J;\n"                                       \
           "è accettato sia  K=x; che K = x;\n" \
           "Devono essere presenti TUTTI i parametri, NON ripetuti\n\n", str);
}

static void cleanup(void) {
    MENO1(unlink(SOCKET_SERVER_NAME))
    /*
     * provo ad ottenere i flag del fd: se li ottengo vuol dire che il fd è attivo,
     *      pertanto provvedo a chiuderlo
     */
    if(fcntl(listen_ssfd, F_GETFL) >= 0)
        MENO1(close(listen_ssfd))

    if(fcntl(smfd, F_GETFL) >= 0)
        MENO1(close(smfd))

    if(fcntl(pipefd_dir[0], F_GETFL) >= 0)
        MENO1(close(pipefd_dir[0]))

    if(fcntl(pipefd_dir[1], F_GETFL) >= 0)
        MENO1(close(pipefd_dir[1]))
}

/***********************************************************************************************
 * LOGICA DEL DIRETTORE (del supermercato)
 * - forka il processo supermercato
 * - gestione segnali
 *      thread signal handler che scrive sulla pipe per avvisare della ricezione del segnale
 * - crea un socket di comunicazione col supermercato e si mette in ascolto su:
 *         . socket di comunicazione
 *         . pipe[0]
 * - gestisce le casse e le uscite dei clienti con 0 prodotti acquistati
 * - quando viene ricevuto un segnale lo reinvia al supermercato
 *      SIGHUP: rimane in attesa di altre comunicazioni dai cassieri/clienti senza acquisti
 *      SIGQUIT: gestisce le richieste sospese
 *  attende la terminazione del supermercato e successivamente termina
 ************************************************************************************************/
int main(int argc, char *argv[]) {
    /***************************************************************************************
     * Controllo parametri di ingresso
    ****************************************************************************************/
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    /** Parametri di configurazione */
    param_t par;

    if (argv[1][0] == '-' && argv[1][2] == '\0') {
        switch (argv[1][1]) {
            case 'c':
                if(argc != 3) {
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                MENO1LIB(get_params_from_file(&par, argv[2]), -1)
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

    /** Variaibili di supporto */
    int err,
        i;

    exit(EXIT_FAILURE);

    /***************************************************************************************
    * Gestione segnali
     * - inizializza la pipe di comunicazione fra signal handler e main
     * - thread signal handler, gestirà SIGQUIT e SIGHUP
     *      maschero tutti i segnali nel thread main
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
     * inizializzazione PIPE per tsh
     */
    MENO1(pipe(pipefd_dir))

    pthread_t tid_tsh;
    void *status_tsh;
    PTH(err, pthread_create(&tid_tsh, NULL, sync_signal_handler, (void *) NULL))

    /***************************************************************************************
    * Apertura Supermercato
    * - fork del processo supermercato
    ****************************************************************************************/
    pid_t pid_sm = -1;          /* pid del supermercato */
    int status_sm;              /* conterrà il valore di uscita del processo supermercato */

    switch ( (pid_sm = fork()) ) {
        case 0:                 /* figlio */
            execl(PATH_TO_SUPERMARKET, "supermercato", argv[2], (char *) NULL);
            perror("exec");
            exit(EXIT_FAILURE);
            break;
        case -1:                /* errore */
            perror("fork");
            exit(EXIT_FAILURE);
            break;
        default:                /* padre */
#ifdef DEBUG
            printf("[DIRETTORE] Supermercato aperto!\n");
#endif
            break;
    }


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

    tmp.fd = pipefd_dir[0];
    tmp.events = POLLIN;
    MENO1(pollfd_add(&pollfd_v, tmp, &polled_fd))

    /*
     * gestione casse
     */
    int *code_casse;            /* conterrà le informazioni riguardo le code delle casse */
    EQNULL(code_casse = malloc(par.K * sizeof(int)))
    for(i = 0; i < par.K; i++)
        code_casse[i] = -1;     /* -1 indica cassa chiusa */

    /***************************************************************************************
     * MULTIPLEXING, tramite POLL:
     *  attendo I/O su più file descriptors
     *      .smfd
     *      .pipe[0]
     *  Inizialmente attende UNA unica richiesta di connessione del supermercato
     *
     *  Può ricevere comunicazioni da:
     *
     *      SIGNAL HANDLER  riceve un segnale, lo rimanda al supermercato
     *
     *      CLIENTI         in caso di uscita senza acquisti
     *                          li gestisce accettando la loro uscita
     *
     *      CASSE           riceve comunicazione riguardo il numero di clienti in coda
     *
     *      MANAGER         segnala l'imminente terminazione del supermercato
     *
     * Quando viene ricevuto un segnale lo reinvia al supermercato
     *    SIGHUP   rimane in attesa di altre comunicazioni dai cassieri/clienti senza acquisti
     *    SIGQUIT  gestisce le richieste sospese
     * attende la terminazione del supermercato e successivamente termina
    ****************************************************************************************/
#ifdef DEBUG
    printf("[DIRETTORE] In attesa di una connessione\n");
#endif
    int sotto_soglia_S1     = 0,
        num_casse_aperte    = 0;

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

                if(fd_curr == pipefd_dir[0]) {
                    /*
                     * il signal handler ha ricevuto un segnale
                     */
                    int sig_captured;
                    MENO1(readn(pipefd_dir[0], &sig_captured, sizeof(int)))
                    /*
                     * manda il segnale ricevuto al supermercato
                     */
#ifdef DEBUG
                    printf("\n[DIRETTORE] ricevuto Segnale [%d]!\n", sig_captured);
#endif
                    // set_stato_supermercato(CHIUSURA_IMMEDIATA);
                    MENO1(kill(pid_sm, sig_captured))

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

                    MENO1(shutdown(listen_ssfd, SHUT_RDWR))
                    MENO1(close(listen_ssfd))

                }
                else {
                    /*
                     * ricevuta comunicazione dal supermercato
                     */
                    sock_msg_code_t type_msg = 0;        /* tipo messaggio ricevuto */
                    int param;                       /* parametro del messaggio */
                    MENO1(readn(smfd, &type_msg, sizeof(sock_msg_code_t)))
#ifdef DEBUG
                    printf("[DIRETTORE] SOCKET [%d]!\n", type_msg);
#endif
                    switch(type_msg) {
                        case MANAGER_IN_CHIUSURA: {
                                MENO1(waitpid(pid_sm, &status_sm, 0))
#ifdef DEBUG
                                printf("[DIRETTORE] Il supermercato è terminato!\n");
#endif
                                goto terminazione_direttore;
                            }
                        case CLIENTE_SENZA_ACQUISTI:
                            /*
                             * assumo che il direttore faccia SEMPRE uscire i clienti
                             *  che non effettuano acquisti
                             *  -leggo l'id cliente
                             */
                            MENO1(readn(smfd, &param, sizeof(int)))
                            type_msg = DIRETTORE_PERMESSO_USCITA;
                            MENO1(writen(smfd, &type_msg, sizeof(sock_msg_code_t)))
                            MENO1(writen(smfd, &param, sizeof(int)))

#ifdef DEBUG
                            printf("[DIRETTORE] il cliente [%d] può uscire!\n", param);
#endif
                            break;
                        case CASSIERE_NUM_CLIENTI: {
                            int ind;
                            /*
                             * leggo l'indice della coda e il numero di clienti in coda
                             */
                            MENO1(readn(smfd, &ind, sizeof(int)))
                            MENO1(readn(smfd, &param, sizeof(int)))
                            /* param >= 0, ind >= 0 */
#ifdef DEBUG_NOTIFY
                            printf("[DIRETTORE] La cassa [%d] ha [%d] clienti in coda!\n", ind, param);
#endif
                            if(code_casse[ind] < 0) { // era chiusa
                                num_casse_aperte++;
                                if(param <= 1)
                                    sotto_soglia_S1++;
                            } else if(code_casse[ind] <= 1) { // se era APERTA e sotto la soglia S1
                                if (param > 1)
                                    sotto_soglia_S1--;
                            } else if(param <= 1)     // non era sotto la soglia S1, ora sì
                                sotto_soglia_S1++;
#ifdef DEBUG_CASSIERE
                            printf("[DIRETTORE] sotto_soglia_S1 [%d]!\n", sotto_soglia_S1);
#endif
                            code_casse[ind] = param;
/*
 * Il direttore, sulla base delle informazioni ricevute dai cassieri, decide se aprire o
 * chiudere casse (al massimo le casse aperte sono K, ed almeno 1 cassa deve rimanere aperta).
 * La decisione viene presa sulla base di alcuni valori soglia S1 ed S2 definiti dallo
 * studente nel file di configurazione. S1 stabilisce la soglia per la chiusura di
 * una cassa, nello specifico, definisce il numero di casse con al più un cliente in coda
 * (es. S1=2: chiude una cassa se ci sono almeno 2 casse che hanno al più un cliente).
 * S2 stabilisce la soglia di apertura di una cassa, nello specifico, definisce il numero di
 * clienti in coda in almeno una cassa (es. S2=10: apre una cassa (se possibile) se
 * c’è almeno una cassa con almeno 10 clienti in coda).
 */
                            /*
                             * decisione CHIUSURA casse
                             */

                            if(num_casse_aperte > 1) {
                                if(sotto_soglia_S1 >= par.S1) {
                                    type_msg = DIRETTORE_CHIUSURA_CASSA;
                                    MENO1(writen(smfd, &type_msg, sizeof(sock_msg_code_t)))
                                    MENO1(writen(smfd, &ind, sizeof(int)))
#ifdef DEBUG_NOTIFY
                                    printf("[DIRETTORE] Chiude cassa [%d]\n", ind);
#endif
                                    code_casse[ind] = -1;
                                    sotto_soglia_S1--;
                                    num_casse_aperte--;
                                }
                            }
                            /*
                             * decisione APERTURA casse
                             */
                            if(code_casse[ind] >= par.S2) {
                                type_msg = DIRETTORE_APERTURA_CASSA;
                                MENO1(writen(smfd, &type_msg, sizeof(sock_msg_code_t)))
#ifdef DEBUG_NOTIFY
                                printf("[DIRETTORE] Apre una cassa\n");
#endif
                            }
                        }
                        default: ;
                        }
                    }
                }
            }
        }

    /***************************************************************************************
     * Terminazione DIRETTORE
    **********************;*****************************************************************/

terminazione_direttore:
    free(code_casse);
    pollfd_destroy(pollfd_v);

    PTH(err, pthread_kill(tid_tsh, SIGUSR1))
    PTH(err, pthread_join(tid_tsh, &status_tsh))
    PTHJOIN(status_tsh, "Signal Handler Direttore")

#ifdef DEBUG_TERM
    printf("[DIRETTORE] CHIUSURA CORRETTA\n");
#endif

    return 0;
}
