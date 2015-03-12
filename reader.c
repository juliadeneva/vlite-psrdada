#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ipcio.h"
#include "dada_def.h"
#include "Connection.h"
#include "def.h"
#include "vdifio.h"

void usage ()
{
  fprintf(stdout,"Usage: reader [options]\n"
	  "-k hexadecimal shared memory key  (default: %x)\n"
	  "-p listening port number (default: %"PRIu64")\n",DADA_DEFAULT_BLOCK_KEY,READER_SERVICE_PORT);
}

int main(int argc, char** argv)
{
  ipcio_t data_block = IPCIO_INIT;
  key_t key = DADA_DEFAULT_BLOCK_KEY;
  int status = 0, arg;
  uint64_t port = READER_SERVICE_PORT;
  char src[SRCMAXSIZE];

  char buf[VDIF_PKT_SIZE];
  int nbytes = 0;

  int state = STATE_STOPPED, cmd = CMD_NONE, ii;

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
	fprintf (stderr, "Reader: could not parse key from %s\n", optarg);
	return -1;
      }
      break;
      
    case 'p':
      if (sscanf (optarg, "%"PRIu64"", &port) != 1) {
	fprintf (stderr, "Reader: could not parse port from %s\n", optarg);
	return -1;
      }
      break;
    }
  }

  if(ipcio_connect(&data_block,key) < 0)
    exit(1);
  fprintf(stderr,"after ipcio_connect\n");

  //create listening socket 
  if(serve(port, &c) < 0) {
    fprintf(stderr,"Reader: Failed to create listening socket.\n");
    exit(1);
  }
  
  //enter started state immediately
  /*
  state = STATE_STARTED;
  if(ipcio_open(&data_block,'R') < 0)
    exit(1);
  fprintf(stderr,"after ipcio_open, R\n");
  */

  sprintf(src,"NONE");
  
  //ii = 0;
  while(1) {
    //ii++;
    //printf("state: %d cmd: %d\n",state,cmd);

    if(state == STATE_STOPPED) {
      
      if(cmd == CMD_NONE)
	cmd = wait_for_cmd(&c,src);

      if(cmd == CMD_START) {
	state = STATE_STARTED;
	fprintf(stderr,"before ipcio_open\n");
	if(ipcio_open(&data_block,'R') < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_open, R\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_STOP) {
	fprintf(stderr,"Reader: ignored cmd stop (already stopped).\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_QUIT) {
	shutdown(c.rqst,2);
	return 0;
      }
      else if(cmd == CMD_EVENT) {
	fprintf(stderr,"Reader: ignored cmd event.\n");
	cmd = CMD_NONE;
	continue;
      }
    }

    if(state == STATE_STARTED) {
      FD_ZERO(&readfds);
      FD_SET(c.rqst, &readfds);
      select(c.rqst+1,&readfds,NULL,NULL,&tv);
      
      //if input is waiting on listening socket, read it
      if(FD_ISSET(c.rqst,&readfds))
	cmd = wait_for_cmd(&c, src);
      
      //if command is stop, change state to STOPPED, close data block
      if(cmd == CMD_STOP) {
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
      else if(cmd == CMD_EVENT) {
	fprintf(stderr,"Reader: ignored cmd event.\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_START) {
	fprintf(stderr,"Reader: ignored cmd start (already started).\n");
	cmd = CMD_NONE;
      }
    
      nbytes = ipcio_read(&data_block,buf,VDIF_PKT_SIZE);
      //fprintf(stderr,"ipcio_read: %d bytes\n",nbytes);
      //print VDIF frame header summary
      //printVDIFHeader((vdif_header *)buf, VDIFHeaderPrintLevelColumns);
      //printVDIFHeader((vdif_header *)buf, VDIFHeaderPrintLevelShort);
      //fflush(stdout);
      //fprintf(stderr,"VDIFFrame MJD: %d Number: %d\n",getVDIFFrameMJD((vdif_header *)buf), getVDIFFrameNumber((vdif_header *)buf));

      /*
      status = fwrite(buf,1,nbytes,outfd);
      fprintf(stderr,"fwrite: %d bytes\n",status);
      */

      //ipcio_read waits if there's no EOD (end of data) written to the last unfilled subblock, so this condition is only met on EOD:
      if(nbytes < VDIF_PKT_SIZE) {
	fprintf(stderr,"Reader: EOD found.\n");

	if(ipcio_close(&data_block) < 0)
	  exit(1);
	fprintf(stderr,"after ipcio_close\n");
	state = STATE_STOPPED;
	cmd = CMD_START;
      }
    }
  }

  //fclose(outfd);
  return 0;
}
