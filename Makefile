INCDIR = /usr/local/src/psrdada/src
LIBDIR = /usr/local/src/psrdada/src
FLAGS = -g

IPCFILES = $(INCDIR)/ipcio.c $(INCDIR)/ipcbuf.c $(INCDIR)/ipcutil.c
SOCKFILES = $(INCDIR)/sock.c

all:	dada_db_simple writer reader

dada_db_simple: dada_db_simple.c 
	gcc -o $@ dada_db_simple.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

writer:	writer.c Connection.h def.h utils.c
	gcc -o $@ writer.c utils.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

reader:	reader.c Connection.h def.h utils.c
	gcc -o $@ reader.c utils.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

clean:
	rm *.o *~ reader writer dada_db_simple

#init:	init.c
#	gcc -o $@ init.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

#udp-send2:udp-send2.c
#	gcc -o $@ udp-send2.c $(SOCKFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

#udp-send:udp-send.c
#	gcc -o $@ udp-send.c -I$(INCDIR) -L$(LIBDIR) $(FLAGS) 