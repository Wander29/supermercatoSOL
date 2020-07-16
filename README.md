# Utilizzo

Il makefile presente nel progetto contiene il necessario per compilare e testare il programma, usare:

 - **make** per compilare ed ottenere gli eseguibili
 - make **clean** / **cleanall** per pulire dai file di lavoro usare clean, per ricompilare l’intero progetto usare cleanall (include l’effetto di clean)
 - make **test1** **test2**

La funzione di cambio cassa dei clienti in coda utilizza in genere molta CPU, è possibile disabilitarla
settando **DFLAGS = -DNO_CAMBIO_CASSA** nel makefile.

Per una descrizione dei parametri aggiuntivi (A, J, ...) e del file di configurazione accettato si prega
di eseguire make **help**

Test1:
 *Il target test1 deve far partire il processo direttore, quindi il processo
supermercato con i seguenti parametri di configurazioni (quelli non specificati, sono a scelta dello studente):
K=2, C=20, E=5, T=500, P=80, S=30 (gli altri parametri a scelta dello studente). Il processo supermercato deve
essere eseguito con valgrind utilizzando il seguente comando: “valgrind –leak-check=full”. Dopo 15s deve
inviare un segnale SIGQUIT al processo direttore. Il test si intende superato se valgrind non riporta errori né
memoria non deallocata (il n. di malloc deve essere uguale al n. di free).*

Test2:
*Il test2 deve lanciare il proccesso direttore che provvederà ad aprire il supermercato lanciando il processo
supermercato con i seguenti parametri (gli altri a scelta dello studente): K=6, C=50, E=3, T=200, P=100, S=20.
Dopo 25s viene inviato un segnale SIGHUP, quindi viene lanciato lo script Bash di analisi ( analisi.sh) che, al
termine dell’esecuzione del supermercato, fornirà sullo standard output un sunto delle statistiche relative
all’intera simulazione appena conclusa. Il test si intende superato se non si producoro errori a run-time e se il
sunto delle statistiche relative alla simulazione riporta “valori ragionevoli” (cioè, non ci sono valori negativi,
valori troppo alti, campi vuoti, etc...).*


