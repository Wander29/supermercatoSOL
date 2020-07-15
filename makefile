CC          =   gcc
CFLAGS      =   -std=c99 -g -Wall -pedantic -Wextra \
				   -Wformat=2 -Wno-unused-parameter -Wshadow \
				   -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
				   -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
.PHONY      =   clean
SRCDIR      =   src/
LIBDIR      =   lib/
LIBSRC		= 	$(LIBDIR)lib-src/
LIBS        =	-lqueue_linked -lpthread -lmyutils -lmypoll -lsupermercato -lsupermercato_com -lpool
LDFLAGS     = 	-Wl,-rpath,$(LIBDIR)
LIBINCLUDE  =   $(LIBDIR)lib-include/
INCDIR		=	include/
INCLUDE     =   $(INCDIR) -I $(LIBINCLUDE)

LIBDIRETTORE =	$(LIBDIR)libmyutils.so $(LIBDIR)libmypoll.so $(LIBDIR)libsupermercato_com.a
LIBSUPERM	=	$(LIBDIRETTORE) $(LIBDIR)libqueue_linked.so $(LIBDIR)libpool.so $(LIBDIR)libsupermercato.a
INCUSED		=	$(LIBINCLUDE)mysocket.h $(LIBINCLUDE)mypthread.h $(INCDIR)mytypes.h

BINDIR		=	bin/

TARGETS     =  	$(BINDIR)direttore
LOGNAME 	=	log.csv

.PHONY: all clean kill test1 test2 test3
.SUFFIXES: .c .o .h

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -I $(INCLUDE)  -c -o $@ $<

$(BINDIR)direttore: $(SRCDIR)direttore.c $(BINDIR)supermercato $(LIBDIRETTORE)
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

$(BINDIR)supermercato: 	$(SRCDIR)supermercato.c $(INCDIR)supermercato.h $(LIBSUPERM) $(INCUSED)
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

$(LIBDIR)libmyutils.so: $(LIBSRC)myutils.c $(LIBINCLUDE)myutils.h
	-rm -f myutils.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -fPIC -o myutils.o $<
	$(CC) -shared -o $@ myutils.o
	rm -f myutils.o

$(LIBDIR)libqueue_linked.so: $(LIBSRC)queue_linked.c $(LIBINCLUDE)queue_linked.h
	-rm -f queue_linked.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -fPIC -o queue_linked.o $<
	$(CC) -shared -o $@ queue_linked.o
	rm -f queue_linked.o

$(LIBDIR)libmypoll.so: $(LIBSRC)mypoll.c $(LIBINCLUDE)mypoll.h
	-rm -f mypoll.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -fPIC -o mypoll.o $<
	$(CC) -shared -o $@ mypoll.o
	rm -f mypoll.o

$(LIBDIR)libparser_config.so: $(LIBSRC)parser_config.c $(LIBINCLUDE)parser_config.h
	-rm -f parser_config.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -fPIC -o parser_config.o $<
	$(CC) -shared -o $@ parser_config.o
	rm -f parser_config.o

$(LIBDIR)libpool.so: $(LIBSRC)pool.c $(LIBINCLUDE)pool.h
	-rm -f pool.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -fPIC -o pool.o $<
	$(CC) -shared -o $@ pool.o
	rm -f pool.o

$(LIBDIR)libsupermercato_com.a: 	$(SRCDIR)protocollo.c $(SRCDIR)parser_writer.c \
									$(INCDIR)protocollo.h $(INCDIR)parser_writer.h
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o protocollo.o $(SRCDIR)protocollo.c
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o parser_writer.o $(SRCDIR)parser_writer.c
	ar rvs $@ protocollo.o parser_writer.o
	rm -f protocollo.o parser_writer.o

$(LIBDIR)libsupermercato.a: 	$(SRCDIR)cliente.c $(SRCDIR)cassiere.c $(SRCDIR)notificatore.c  \
								$(INCDIR)cliente.h $(INCDIR)cassiere.h $(INCDIR)notificatore.h  \
								$(INCDIR)protocollo.h
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o cliente.o $(SRCDIR)cliente.c
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o cassiere.o $(SRCDIR)cassiere.c
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o notificatore.o $(SRCDIR)notificatore.c
	ar rvs $@ cliente.o cassiere.o notificatore.o
	rm -f cliente.o cassiere.o notificatore.o

test1:
	valgrind --trace-children=yes --leak-check=full ./bin/direttore -c ./config/configtest1.txt & \
	sleep 15 ;\
	kill -s QUIT "$$!" ;\
	wait $$!

test2:
	-chmod +x ./script/analisi.sh ;\
	./bin/direttore -c ./config/configtest2.txt & \
	sleep 5 ;\
	kill -s HUP "$$!" ;\
	wait $$! ;\
	./script/analisi.sh $(LOGNAME) | less

clean:
	-rm -f *.o
	-rm -f *.socket
	-rm -f ~*
	-rm -f vgcore*
	-rm -f out*
	-rm -f *.csv

kill: clean
	-killall -r supermercato -r memcheck

cleanall: clean
	-rm -f $(TARGETS)
	-rm -f $(LIBDIR)*.so
	-rm -f $(LIBDIR)*.a