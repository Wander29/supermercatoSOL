CC          =   gcc
CFLAGS      =   -std=c99 -g -Wall -pedantic -Wextra \
				   -Wformat=2 -Wno-unused-parameter -Wshadow \
				   -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
				   -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
.PHONY      =   clean
SRCDIR      =   src/
LIBDIR      =   lib/
LIBSRC		= 	$(LIBDIR)lib-src/
LIBS        =	-lqueue_linked -lpthread -lmyutils -lmypoll -lparser_config -lpool -licl_hash
LDFLAGS     = 	-Wl,-rpath,$(LIBDIR)
LIBINCLUDE  =   $(LIBDIR)lib-include/
INCDIR		=	include/
INCLUDE     =   $(INCDIR) -I $(LIBINCLUDE)
LIBUSED		=	$(LIBDIR)libmyutils.so $(LIBDIR)libqueue_linked.so $(LIBDIR)libmypoll.so \
				$(LIBDIR)libpool.so $(LIBDIR)libparser_config.so $(LIBDIR)libicl_hash.a
INCUSED		=	$(LIBINCLUDE)mysocket.h $(LIBINCLUDE)mypthread.h $(INCDIR)mytypes.h
OBJDIR		=	obj/
OBJS		=	$(OBJDIR)cliente.o $(OBJDIR)cassiere.o $(OBJDIR)protocollo.o
BINDIR		=	bin/

TARGETS     =  	$(BINDIR)direttore

.PHONY: all clean kill
.SUFFIXES: .c .o .h

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -I $(INCLUDE)  -c -o $@ $<

$(BINDIR)direttore: $(SRCDIR)direttore.c $(BINDIR)supermercato $(OBJDIR)protocollo.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS) $(OBJDIR)protocollo.o

$(BINDIR)supermercato: 	$(SRCDIR)supermercato.c $(INCDIR)supermercato.h $(LIBUSED) $(INCUSED) $(OBJS)
	$(CC) $(CFLAGS) -I $(INCLUDE) -L $(LIBDIR) $(LDFLAGS) -o $@ $< $(LIBS) $(OBJS)

$(OBJDIR)cliente.o: $(SRCDIR)cliente.c $(INCDIR)cliente.h $(OBJDIR)protocollo.o
	$(CC) $(CFLAGS) -I $(INCLUDE)  -c -o $@ $<

$(OBJDIR)cassiere.o: $(SRCDIR)cassiere.c $(INCDIR)cassiere.h $(OBJDIR)protocollo.o
	$(CC) $(CFLAGS) -I $(INCLUDE)  -c -o $@ $<

$(OBJDIR)protocollo.o: $(SRCDIR)protocollo.c $(INCDIR)protocollo.h
	$(CC) $(CFLAGS) -I $(INCLUDE)  -c -o $@ $<

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

$(LIBDIR)libicl_hash.a: $(LIBSRC)icl_hash.c $(LIBINCLUDE)icl_hash.h
	-rm -f icl_hash.o
	$(CC) $(CFLAGS) -I $(INCLUDE) -c -o icl_hash.o $<
	ar rvs $@ icl_hash.o
	rm -f icl_hash.o

clean:
	-rm -f *.o
	-rm -f *.socket
	-rm -f ~*
	-rm -f vgcore*

kill: clean
	-killall -r supermercato -r memcheck

cleanall: clean
	-rm -f $(TARGETS)
	-rm -f $(LIBDIR)*.so
	-rm -f $(LIBDIR)*.a
	-rm -f $(OBJS)