# Utilizzo

Il makefile presente nel progetto contiene il necessario per compilare e testare il programma, usare:
• **make**
• make **clean** / **cleanall** per pulire dai file di lavoro usare clean, per ricompilare l’intero progetto
usare cleanall (include l’effetto di clean)
• make **test1** **test2**
La funzione di cambio cassa dei clienti in coda utilizza in genere molta CPU, è possibile disabilitarla
settando **DFLAGS = -DNO_CAMBIO_CASSA** nel makefile.

Per una descrizione dei parametri aggiuntivi (A, J, ...) e del file di configurazione accettato si prega
di eseguire make **help**.

