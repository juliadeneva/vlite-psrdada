INCDIR = /usr/local/src/psrdada/src
LIBDIR = /usr/local/src/psrdada/src
FLAGS = -g

IPCFILES = $(INCDIR)/ipcio.c $(INCDIR)/ipcbuf.c $(INCDIR)/ipcutil.c
SOCKFILES = $(INCDIR)/sock.c

all:	dada_db_simple writer reader messenger

dada_db_simple: dada_db_simple.c 
	gcc -o $@ dada_db_simple.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

writer:	writer.c Connection.h def.h utils.c
	gcc -o $@ writer.c utils.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

reader:	reader.c Connection.h def.h utils.c
	gcc -o $@ reader.c utils.c $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS)

PARSE_FILES = eop.c  executor.c  options.c vlaant.c  vlite_xml.c multicast.c
PARSE_HEADERS = efaults.h  eop.h  executor.h  options.h  vlaant.h  vlite_xml.h multicast.h

messenger: messenger.c utils.c $(PARSE_FILES) 
	gcc -o $@ messenger.c utils.c $(PARSE_FILES) $(IPCFILES) -I$(INCDIR) -L$(LIBDIR) $(FLAGS) -lexpat

clean:
	rm *.o *~ reader writer dada_db_simple messenger

