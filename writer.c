#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dada_def.h"
#include "ipcio.h"
#include "Connection.h"
#include "def.h"

//Instead of reading from socket, for now
char infile[] = "2011-01-15-16:56:35.fil";

void usage ()
{
  fprintf(stdout,"Usage: writer [options]\n"
	  "-k hexadecimal shared memory key  (default: %x)\n"
	  "-p listening port number (default: %d)\n",DADA_DEFAULT_BLOCK_KEY,WRITER_SERVICE_PORT);
}


int main(int argc, char** argv)
{
  ipcio_t data_block = IPCIO_INIT;
  key_t key = DADA_DEFAULT_BLOCK_KEY;
  int status = 0;

  char eventfile[128], src[SRCMAXSIZE];;
  FILE *infd, *evfd;

  //get this many bytes at a time from input file (XXX: later, from data socket)
  int BUFSIZE = 10*VDIF_PKT_SIZE; 
  char buf[BUFSIZE];

  int nbytes = 0, eventcount = 0, state = STATE_STOPPED, port = WRITER_SERVICE_PORT;
  int cmd = CMD_NONE, ii, arg; 
  
  fd_set readfds;
  struct timeval tv; //timeout for select()--set it to zero 
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  Connection c;
  c.sockoptval = 1; //release port immediately after closing connection

  while ((arg = getopt(argc, argv, "hk:p:")) != -1) {
    switch(arg) {

    case 'h':
      usage ();
      return 0;

    case 'k':
      if (sscanf (optarg, "%x", &key) != 1) {
	fprintf (stderr, "writer: could not parse key from %s\n", optarg);
	return -1;
      }
      break;
      
    case 'p':
      if (sscanf (optarg, "%"PRIu64"", &port) != 1) {
	fprintf (stderr, "writer: could not parse port from %s\n", optarg);
	return -1;
      }
      break;
    }
  }

  if((infd = fopen(infile, "r")) == NULL) {
    fprintf(stderr,"Writer: Could not open file %s\n",infile);
    exit(1);
  }
  
  //Try to connect to existing data block
  if(ipcio_connect(&data_block,key) < 0)
    exit(1);
  fprintf(stderr,"after ipcio_connect\n");

  //create listening socket 
  if(serve(port, &c) < 0) {
    fprintf(stderr,"Writer: Failed to create listening socket.\n");
    exit(1);
  }
    
  ii = 0;
  sprintf(src,"NONE");

  //in final version, this loop will be while(1)
  while(!feof(infd)) {
    ii++;
    printf("ii: %d state: %d cmd: %d\n",ii,state,cmd);

    if(state == STATE_STOPPED) {
      if(cmd == CMD_NONE)
	cmd = wait_for_cmd(&c,src);

      if(cmd == CMD_START) {
	state = STATE_STARTED;
	if(ipcio_open(&data_block,'W') < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_open, W\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_EVENT) {
	sprintf(eventfile,"%s_event_%04d.out",src,eventcount);
	if((evfd = fopen(eventfile,"w")) == NULL) {
	  fprintf(stderr,"Writer: Could not open file %s for writing.\n",eventfile);
	  exit(1);
	}
	event_to_file(&data_block,evfd);
	fclose(evfd);
	eventcount++;
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_STOP) {
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_QUIT) {
	shutdown(c.rqst,2);
	return 0;
      }
    }
    
    //if state is STARTED, poll the command listening socket (in final version, select() will poll both the command listening socket and the data socket)
    if(state == STATE_STARTED) {
      //why do the FD macros have to be called before each select() call instead of just once outside the loop? 
      FD_ZERO(&readfds);
      FD_SET(c.rqst, &readfds);
      select(c.rqst+1,&readfds,NULL,NULL,&tv);
      
      //if input is waiting on listening socket, read it
      if(FD_ISSET(c.rqst,&readfds)) {
	cmd = wait_for_cmd(&c,src);
      }

      if(cmd == CMD_EVENT) {
	//close data block
	if(ipcio_close(&data_block) < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_close\n");
	
	//dump DB to file
	sprintf(eventfile,"%s_event_%04d.out",src,eventcount);
	if((evfd = fopen(eventfile,"w")) == NULL) {
	  fprintf(stderr,"Writer: Could not open file %s for writing.\n",eventfile);
	  exit(1);
	}
	event_to_file(&data_block,evfd);
	fclose(evfd);
	eventcount++;

	state = STATE_STOPPED;
	cmd = CMD_START; 

	//don't write anything to the data block before checking for any pending commands (this can lead to writing a standalone EOD in the data block if there's a stop pending)
	continue; 
      }
      //if command is stop, change state to STOPPED, close data block
      else if(cmd == CMD_STOP) {
	state = STATE_STOPPED;
	if(ipcio_close(&data_block) < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_close\n");
	cmd = CMD_NONE;
	continue;
      }
      //if command is quit, close data block, shutdown listening socket, return
      else if(cmd == CMD_QUIT) {
	if(ipcio_close(&data_block) < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_close\n");
	shutdown(c.rqst,2);
	return 0;
      }
      //XXX: what if command is start before a stop is received? 
      
      //XXX: check for overflow of the data block here?
      nbytes = fread(buf,1,BUFSIZE,infd);
      fprintf(stderr,"fread: %d bytes\n",nbytes);
      status = ipcio_write(&data_block,buf,nbytes);
      fprintf(stderr,"ipcio_write: %d bytes\n",status);
      
      sleep(2);
    }
  }
  
  fclose(infd);
  
  return 0;
}

