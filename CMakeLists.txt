cmake_minimum_required(VERSION 3.16)
project(PROGETTO C)

set(CMAKE_C_STANDARD 99)

add_executable(PROGETTO lib/lib-src/myutils.c include/myutils.h lib/lib-include/mysocket.h lib/lib-src/queue_linked.c include/concurrent_queue.h lib/lib-include/mypthread.h lib/lib-src/mypoll.c include/mypoll.h src/supermercato.c src/parser_writer.c include/parser_writer.h lib/lib-src/pool.c include/pool.h src/direttore.c include/protocollo.h lib/lib-src/protocollo.c src/cliente.c include/cliente.h src/cassiere.c include/cassiere.h include/supermercato.h include/mytypes.h src/notificatore.c include/notificatore.h testa_coda.c)