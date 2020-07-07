#include <stdio.h>
#include <stdlib.h>
#include <myutils.h>

#include <pthread.h>
#include <mypthread.h>
#include <parser_config.h>
#include <signal.h>

#define DEBUG


typedef enum stato_supermercato {
    APERTO = 0,
    CHIUSURA_IMMEDIATA,
    CHIUSURA_SOFT;
} stato_sm_t;

/* Var. GLOBALI*/
stato_sm_t stato_suermercato;

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
 * si termina il programma attraverso la ricezione di segnali:
 * - SIGQUIT    terminazione coatta
 * - SIGHUP     terminazione soft
 *********************************************************************************************************/

void *executor(void *arg);
void *cassiere(void *arg);

int main() {
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

    /* Variabili di supporto */
    int i,                      /* indice dei cicli */
        err;                    /* conterrà i valori di ritorno di alcune chiamate (es: pthread) */

    /*************************************************************
    * Generazione cassieri
    *  - thread pool di K cassieri
    *  - ne attivo solamente J
    *************************************************************/
    pthread_t *tid_casse;       /* tid dei cassieri*/
    EQNULL(tid_casse = calloc(par.K, sizeof(pthread_t)))
    void **status_casse;        /* conterrà i valori di ritorno dei thread cassieri */
    EQNULL(status_casse = calloc(par.K, sizeof(void *)))

    threadpool_set_t arg;     /* conterrà i parametri per far attesa e lavoro del cassiere */
    pool_start(&arg);
    z

    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_create((tid_casse + i), NULL, executor, (void *) &arg))
    }
    
    /*
    for(i = 0; i < par.J; i++) {
        PTH(err, pthread_cond_signal(&(arg.cond)))
    }
     */

    /*************************************************************
    * Terminazione: cleanup
    *************************************************************/
    for(i = 0; i < par.K; i++) {
        PTH(err, pthread_join(tid_casse[i], status_casse+i))
        if(status_casse[i] == PTHREAD_CANCELED) {
            printf("Thread cassiere cancellato");
        }
        else if( status_casse[i] != (void *) 0) {
            perror("Thread exited");
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
 * - la cassa è aperta
 * - spawna un thread secondario avente il compito di comunicare col direttore
 *          segretario_cassiere()
 * - attende che ci siano clienti
 * - se ci sono clienti in coda li serve
 **********************************************************************************************************/
void *cassiere(void *arg) {
#ifdef DEBUG
    printf("sono nel CASSIERE! [%d]\n", (int) arg);
#endif

    return (void *) 0;
}