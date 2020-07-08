CC          =   gcc
CFLAGS      =   -std=c99 -g -Wall
.PHONY      =   clean
SRCDIR      =   src/
LIBDIR      =   lib/
LIBSRC		= 	$(LIBDIR)lib-src/
LIBS        =	-lprotocollo -lconcurrent_queue -lpthread -lmyutils -lmypoll -lparser_config -lpool
LDFLAGS     = 	-Wl,-rpath,$(LIBDIR)
INCDIR      =   include/

TARGETS     =  	supermercato					\
				direttore
.PHONY: all clean
.SUFFIXES: .c .o .h

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -I $(INCDIR) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I $(INCDIR) -c -o $@ $<

direttore: 		$(SRCDIR)direttore.c \
				$(LIBDIR)libmyutils.so $(LIBDIR)libmypoll.so $(LIBDIR)libprotocollo.a \
				$(LIBDIR)libparser_config.so \
				$(INCDIR)parser_config.h $(INCDIR)mysocket.h $(INCDIR)mypthread.h

	$(CC) $(CFLAGS) -I $(INCDIR) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

supermercato: 	$(SRCDIR)supermercato.c \
				$(LIBDIR)libmyutils.so $(LIBDIR)libconcurrent_queue.so $(LIBDIR)libmypoll.so \
				$(LIBDIR)libpool.so $(LIBDIR)libprotocollo.a $(LIBDIR)libparser_config.so \
				$(INCDIR)parser_config.h $(INCDIR)mysocket.h $(INCDIR)mypthread.h

	$(CC) $(CFLAGS) -I $(INCDIR) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

$(LIBDIR)libmyutils.so: $(LIBSRC)myutils.c $(INCDIR)myutils.h
	-rm -f myutils.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -fPIC -o myutils.o $<
	$(CC) -shared -o $@ myutils.o
	rm -f myutils.o

$(LIBDIR)libconcurrent_queue.so: $(LIBSRC)concurrent_queue.c $(INCDIR)concurrent_queue.h
	-rm -f concurrent_queue.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -fPIC -o concurrent_queue.o $<
	$(CC) -shared -o $@ concurrent_queue.o
	rm -f concurrent_queue.o

$(LIBDIR)libmypoll.so: $(LIBSRC)mypoll.c $(INCDIR)mypoll.h
	-rm -f mypoll.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -fPIC -o mypoll.o $<
	$(CC) -shared -o $@ mypoll.o
	rm -f mypoll.o

$(LIBDIR)libparser_config.so: $(LIBSRC)parser_config.c $(INCDIR)parser_config.h
	-rm -f parser_config.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -fPIC -o parser_config.o $<
	$(CC) -shared -o $@ parser_config.o
	rm -f parser_config.o

$(LIBDIR)libpool.so: $(LIBSRC)pool.c $(INCDIR)pool.h
	-rm -f pool.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -fPIC -o pool.o $<
	$(CC) -shared -o $@ pool.o
	rm -f pool.o

$(LIBDIR)libprotocollo.a: $(LIBSRC)protocollo.c $(INCDIR)protocollo.h
	-rm -f protocollo.o
	$(CC) $(CFLAGS) -I $(INCDIR) -c -o protocollo.o $<
	ar rvs $@ protocollo.o
	rm -f protocollo.o

clean:
	-rm -f *.o
	-rm -f $(TARGETS)
	-rm -f $(LIBDIR)*.so
	-rm -f $(LIBDIR)*.a