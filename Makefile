INCDIR = /home/vlite-master/frb-search/psrdada/src
#LIBDIR = /users/vlitenrl/home/src/psrdada/src
FLAGS = -g
INCDIFXMESSAGE =  /home/vlite-master/frb-search/difxmessage-2.4.0
VDIFIODIR = /home/vlite-master/vlite/difx/VLITE-TRUNK/src/vdifio/src

IPCFILES = $(INCDIR)/ipcio.c $(INCDIR)/ipcbuf.c $(INCDIR)/ipcutil.c
SOCKFILES = $(INCDIR)/sock.c
VDIFIOFILES = $(VDIFIODIR)/vdifio.c

all:	dada_db_simple writer reader messenger db_unwrap

dada_db_simple: dada_db_simple.c 
	gcc -o $@ dada_db_simple.c $(IPCFILES) -I$(INCDIR) $(FLAGS)
	chmod a+rx $@

writer:	writer.c Connection.h def.h utils.c
	gcc -o $@ writer.c utils.c $(IPCFILES) $(VDIFIOFILES) -I$(INCDIR) -I$(VDIFIODIR) $(FLAGS) 
	chmod a+rx $@

reader:	reader.c Connection.h def.h utils.c
	gcc -o $@ reader.c utils.c $(IPCFILES) $(VDIFIOFILES) -I$(INCDIR) -I$(VDIFIODIR) $(FLAGS)
	chmod a+rx $@

PARSE_FILES = eop.c  executor.c  options.c vlaant.c  vlite_xml.c multicast.c alert.c
PARSE_HEADERS = efaults.h  eop.h  executor.h  options.h  vlaant.h  vlite_xml.h multicast.h alert.h

messenger: messenger.c utils.c $(PARSE_FILES) 
	gcc -o $@ messenger.c utils.c $(PARSE_FILES) $(IPCFILES) -I$(INCDIR)  $(FLAGS) -lexpat -I$(INCDIFXMESSAGE)
	chmod a+rx $@

db_unwrap: db_unwrap.c def.h
	gcc -o $@ db_unwrap.c $(VDIFIOFILES) -I$(VDIFIODIR) $(FLAGS) 
	chmod a+rx $@

clean:
	rm *.o *~ reader writer dada_db_simple messenger db_unwrap

