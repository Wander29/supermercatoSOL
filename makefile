CC          =   gcc
CFLAGS      =   -std=c99 -g -Wall -pedantic -Wextra \
				   -Wformat=2 -Wno-unused-parameter -Wshadow \
				   -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
				   -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
DFLAGS      =   #-DNO_CAMBIO_CASSA
FLAGS       =   $(CFLAGS) $(DFLAGS)
.PHONY      =   clean
SRCDIR      =   src/
LIBDIR      =   lib/
LIBSRC		= 	$(LIBDIR)lib-src/
LIBS        =	-lpthread -lmyutils -lmypoll -lsupermercato -lsupermercato_com -lpool -lqueue_linked
LDFLAGS     = 	-Wl,-rpath,$(LIBDIR)
LIBINCLUDE  =   $(LIBDIR)lib-include/
INCDIR		=	include/
INCLUDE     =   $(INCDIR) -I $(LIBINCLUDE)

LIBDIRETTORE =	$(LIBDIR)libmyutils.so $(LIBDIR)libmypoll.so $(LIBDIR)libsupermercato_com.a
LIBSUPERM	=	$(LIBDIRETTORE) $(LIBDIR)libqueue_linked.so $(LIBDIR)libpool.so $(LIBDIR)libsupermercato.a
INCUSED		=	$(LIBINCLUDE)mysocket.h $(LIBINCLUDE)mypthread.h $(INCDIR)mytypes.h $(INCDIR)protocollo.h

BINDIR		=	bin/

TARGETS     =  	$(BINDIR)direttore
CONFIGFILE  =   config.txt
LOGNAME 	=	log.csv

.PHONY: all clean cleanall test1 test2 help
.SILENT: help test1 test2

all: $(TARGETS)

%: %.c
	$(CC) $(FLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c %.h
	$(CC) $(FLAGS) -I $(INCLUDE)  -c -o $@ $<

$(BINDIR)direttore: $(SRCDIR)direttore.c $(BINDIR)supermercato $(LIBDIRETTORE)
	$(CC) $(FLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

$(BINDIR)supermercato: 	$(SRCDIR)supermercato.c $(INCDIR)supermercato.h $(LIBSUPERM) $(INCUSED)
	$(CC) $(FLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

$(LIBDIR)libmyutils.so: $(LIBSRC)myutils.c $(LIBINCLUDE)myutils.h
	-rm -f myutils.o
	$(CC) $(FLAGS) -I $(INCLUDE) -c -fPIC -o myutils.o $<
	$(CC) -shared -o $@ myutils.o
	rm -f myutils.o

$(LIBDIR)libqueue_linked.so: $(LIBSRC)queue_linked.c $(LIBINCLUDE)queue_linked.h
	-rm -f queue_linked.o
	$(CC) $(FLAGS) -I $(INCLUDE) -c -fPIC -o queue_linked.o $<
	$(CC) -shared -o $@ queue_linked.o
	rm -f queue_linked.o

$(LIBDIR)libmypoll.so: $(LIBSRC)mypoll.c $(LIBINCLUDE)mypoll.h
	-rm -f mypoll.o
	$(CC) $(FLAGS) -I $(INCLUDE) -c -fPIC -o mypoll.o $<
	$(CC) -shared -o $@ mypoll.o
	rm -f mypoll.o

$(LIBDIR)libpool.so: $(LIBSRC)pool.c $(LIBINCLUDE)pool.h
	-rm -f pool.o
	$(CC) $(FLAGS) -I $(INCLUDE) -c -fPIC -o pool.o $<
	$(CC) -shared -o $@ pool.o
	rm -f pool.o

$(LIBDIR)libsupermercato_com.a: $(SRCDIR)parser_writer.c $(INCDIR)parser_writer.h $(INCDIR)protocollo.h
	$(CC) $(FLAGS) -I $(INCLUDE) -c -o parser_writer.o $(SRCDIR)parser_writer.c
	ar rvs $@ parser_writer.o
	rm -f parser_writer.o

$(LIBDIR)libsupermercato.a: 	$(SRCDIR)cliente.c $(SRCDIR)cassiere.c $(SRCDIR)notificatore.c  \
								$(INCDIR)cliente.h $(INCDIR)cassiere.h $(INCDIR)notificatore.h  \
								$(INCDIR)protocollo.h
	$(CC) $(FLAGS) -I $(INCLUDE) -c -o cliente.o $(SRCDIR)cliente.c
	$(CC) $(FLAGS) -I $(INCLUDE) -c -o cassiere.o $(SRCDIR)cassiere.c
	$(CC) $(FLAGS) -I $(INCLUDE) -c -o notificatore.o $(SRCDIR)notificatore.c
	ar rvs $@ cliente.o cassiere.o notificatore.o
	rm -f cliente.o cassiere.o notificatore.o

test1:
	echo "C=20;" 	>$(CONFIGFILE);\
	echo "K=2;"  	>>$(CONFIGFILE);\
	echo "E=5;"  	>>$(CONFIGFILE);\
	echo "T=500;"  	>>$(CONFIGFILE);\
	echo "P=80;"  	>>$(CONFIGFILE);\
	echo "S=30;"  	>>$(CONFIGFILE);\
	echo "L=3;"  	>>$(CONFIGFILE);\
	echo "J=1;"  	>>$(CONFIGFILE);\
	echo "A=400;"  	>>$(CONFIGFILE);\
	echo "S1=2;"  	>>$(CONFIGFILE);\
	echo "S2=6;"  	>>$(CONFIGFILE);\
	echo "Z=$(LOGNAME);"  	>>$(CONFIGFILE);\
	valgrind --trace-children=yes --leak-check=full $(BINDIR)direttore -c $(CONFIGFILE) & \
	sleep 15 ;\
	kill -s QUIT "$$!" ;\
	wait $$!

test2:
	-chmod +x ./script/analisi.sh ;\
	echo "C=50;" 	>$(CONFIGFILE);\
	echo "K=6;"  	>>$(CONFIGFILE);\
	echo "E=3;"  	>>$(CONFIGFILE);\
	echo "T=200;"  	>>$(CONFIGFILE);\
	echo "P=100;"  	>>$(CONFIGFILE);\
	echo "S=20;"  	>>$(CONFIGFILE);\
	echo "L=3;"  	>>$(CONFIGFILE);\
	echo "J=2;"  	>>$(CONFIGFILE);\
	echo "A=400;"  	>>$(CONFIGFILE);\
	echo "S1=2;"  	>>$(CONFIGFILE);\
	echo "S2=8;"  	>>$(CONFIGFILE);\
	echo "Z=$(LOGNAME);"  	>>$(CONFIGFILE);\
	$(BINDIR)direttore -c $(CONFIGFILE) & \
	sleep 25 ;\
	kill -s HUP "$$!" ;\
	wait $$! ;\
	./script/analisi.sh $(LOGNAME) | less

help:
	$(BINDIR)/direttore -h ;\
	echo ""

clean:
	-rm -f *.o
	-rm -f *.socket
	-rm -f ~*
	-rm -f vgcore*
	-rm -f out*
	-rm -f $(LOGNAME)

cleanall: clean
	-rm -f $(TARGETS)
	-rm -f $(LIBDIR)*.so
	-rm -f $(LIBDIR)*.a

#kill: clean
#	-killall -r supermercato -r memcheck