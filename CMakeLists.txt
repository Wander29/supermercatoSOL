cmake_minimum_required(VERSION 3.16)
project(PROGETTO C)

set(CMAKE_C_STANDARD 99)

add_executable(PROGETTO lib/lib-src/myutils.c include/myutils.h include/mysocket.h lib/lib-src/concurrent_queue.c include/concurrent_queue.h include/mypthread.h lib/lib-src/mypoll.c include/mypoll.h src/supermercato.c lib/lib-src/parser_config.c include/parser_config.h lib/lib-src/pool.c include/pool.h src/direttore.c include/protocollo.h lib/lib-src/protocollo.c)