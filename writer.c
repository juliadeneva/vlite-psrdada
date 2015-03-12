#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "dada_def.h"
#include "ipcio.h"
#include "Connection.h"
#include "def.h"
#include "vdifio.h"

//Instead of reading from socket, for now
char infile[] = "2011-01-15-16:56:35.fil";

void usage ()
{
  fprintf(stdout,"Usage: writer [options]\n"
	  "-k hexadecimal shared memory key  (default: %x)\n"
	  "-p listening port number (default: %"PRIu64")\n"
	  "-e ethernet device id (default: eth0)\n"
	  ,DADA_DEFAULT_BLOCK_KEY,WRITER_SERVICE_PORT);
}


int main(int argc, char** argv)
{
  ipcio_t data_block = IPCIO_INIT;
  key_t key = DADA_DEFAULT_BLOCK_KEY;
  int status = 0;

  char eventfile[128], src[SRCMAXSIZE];
  FILE *infd, *evfd;

  //get this many bytes at a time from data socket
  int BUFSIZE = UDP_HDR_SIZE + VDIF_PKT_SIZE; 
  char buf[BUFSIZE];
  char dev[16] = ETHDEV;
  char hostname[MAXHOSTNAME];
  gethostname(hostname,MAXHOSTNAME);
  char *starthost = strstr(hostname, "difx");
  
  int nbytes = 0, state = STATE_STOPPED;
  uint64_t port = WRITER_SERVICE_PORT;
  int cmd = CMD_NONE, ii, arg, maxsock = 0; 
  
  fd_set readfds;
  struct timeval tv; //timeout for select()--set it to zero 
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  Connection c;
  c.sockoptval = 1; //release port immediately after closing connection

  Connection raw;
  raw.alen = sizeof(raw.rem_addr);
  raw.sockoptval = 32*1024*1024; //size of data socket internal buffer

  // For gathering stats on frame skips
  long framenumtmp, framenum[2], framediff;
  framenum[0] = -1;
  framenum[1] = -1;
  int threadid = -1; 
  FILE* skiplogfd;
  char skiplogfile[128];
  time_t currt;
  char currt_string[128];
  struct tm *tmpt;

  while ((arg = getopt(argc, argv, "hk:p:e:")) != -1) {
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
    
    case 'e':
      if (sscanf (optarg, "%s", dev) != 1) {
	fprintf (stderr, "writer: could not parse ethernet device from %s\n", optarg);
	return -1;
      }
      break;
      
    }
  }

  sprintf(skiplogfile,"%s%s%s%s","skiplog-",starthost,dev,".txt");
  if((skiplogfd = fopen(skiplogfile, "w")) == NULL) {
    fprintf(stderr,"Writer: Could not open skiplog file %s\n",skiplogfile);
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
  maxsock = c.rqst;

  //Open raw data stream socket
  raw.svc = openRawSocket(dev,0);
  if(raw.svc < 0) {
    fprintf(stderr, "Cannot open raw socket on %s. Error code %d\n", dev, raw.svc);
    exit(1);
  }
  setsockopt(raw.svc, SOL_SOCKET, SO_RCVBUF, (char *)&(raw.sockoptval), sizeof(raw.sockoptval));
  if(raw.svc > maxsock)
    maxsock = raw.svc;

  //ii = 0;
  sprintf(src,"NONE");

  //in final version, this loop will be while(1)
  //while(!feof(infd)) {
  while(1) {
    //ii++;
    //printf("ii: %d state: %d cmd: %d\n",ii,state,cmd);

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
	fprintf(stderr, "Writer: ignored CMD_EVENT in STATE_STOPPED.\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_STOP) {
	fprintf(stderr, "Writer: ignored CMD_STOP in STATE_STOPPED.\n");
	cmd = CMD_NONE;
      }
      else if(cmd == CMD_QUIT) {
	shutdown(c.rqst,2);
	return 0;
      }
    }
    
    //if state is STARTED, poll the command listening socket and the VDIF raw data socket
    if(state == STATE_STARTED) {
      FD_ZERO(&readfds);
      FD_SET(c.rqst, &readfds);
      FD_SET(raw.svc, &readfds);
      select(maxsock+1,&readfds,NULL,NULL,&tv);
      
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
	currt = time(NULL);
	tmpt = localtime(&currt);
	strftime(currt_string,sizeof(currt_string), "%Y%m%d_%H%M%S", tmpt);
	*(currt_string+15) = 0;
	sprintf(eventfile,"%s%s_%s_%s_ev.out",starthost,dev,currt_string,src);
	
	if((evfd = fopen(eventfile,"w")) == NULL) {
	  fprintf(stderr,"Writer: Could not open file %s for writing.\n",eventfile);
	  exit(1);
	}
	event_to_file(&data_block,evfd);
	fclose(evfd);

	//Flush out and ignore pending commands as they are out of date, then resume writing to ring buffer
	FD_ZERO(&readfds);
	FD_SET(c.rqst, &readfds);
	FD_SET(raw.svc, &readfds);
	select(maxsock+1,&readfds,NULL,NULL,&tv);

	if(FD_ISSET(c.rqst,&readfds)) {
	  cmd = wait_for_cmd(&c,src);
	  fprintf(stderr,"Writer: flushed out command socket after event_to_file.\n");
	}
	
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
      else if(cmd == CMD_START) {
	fprintf(stderr,"Writer: ignored cmd start (already started).\n");
	cmd = CMD_NONE;
      }
      
      //read a packet from the data socket and write it to the ring buffer
      if(FD_ISSET(raw.svc,&readfds)) {
	nbytes = recvfrom(raw.svc, buf, BUFSIZE, 0, (struct sockaddr *)&(raw.rem_addr), &(raw.alen));

	if(nbytes == BUFSIZE) {
	  //fwrite(buf + trim, recvlen-trim, 1, out);
	  status = ipcio_write(&data_block,buf+UDP_HDR_SIZE,VDIF_PKT_SIZE);
	  //fprintf(stderr,"ipcio_write: %d bytes\n",status);

	  //print VDIF frame header
	  //printVDIFHeader((vdif_header *)(buf+UDP_HDR_SIZE), VDIFHeaderPrintLevelLong);
	  //printVDIFHeader((vdif_header *)(buf+UDP_HDR_SIZE), VDIFHeaderPrintLevelColumns);
	  //printVDIFHeader((vdif_header *)(buf+UDP_HDR_SIZE), VDIFHeaderPrintLevelShort);
	  //fprintf(stderr,"VDIFFrame MJD: %d Number: %d\n",getVDIFFrameMJD((vdif_header *)(buf+UDP_HDR_SIZE)), getVDIFFrameNumber((vdif_header *)(buf+UDP_HDR_SIZE)));
	  framenumtmp = getVDIFFrameNumber((vdif_header *)(buf+UDP_HDR_SIZE));
	  threadid = getVDIFThreadID((vdif_header *)(buf+UDP_HDR_SIZE));
	  framediff = framenumtmp - framenum[threadid];

	  if(framediff != -MAXFRAMENUM && framenum[threadid] != -1 && framediff != 1) {
	  //if(framediff != -25599 && abs(framediff) > 1 && framenum[threadid] != -1) {
	    currt = time(NULL);
	    tmpt = localtime(&currt);
	    strftime(currt_string,sizeof(currt_string), "%Y-%m-%d %H:%M:%S", tmpt);
	    fprintf(skiplogfd,"%s FRAME SKIP FROM %d to %d (THREAD %d)\n",currt_string,framenum[threadid],framenumtmp,threadid);
	    fflush(skiplogfd);
	  }
	  else if(framenum[threadid] == -1) {
	    
	    fprintf(skiplogfd,"Writer: Thread %d First frame: %d\n",threadid,framenumtmp);
	    fflush(skiplogfd);
	  }

	  framenum[threadid] = framenumtmp;

	}
	else if(nbytes <= 0) {
	  fprintf(stderr,"Raw socket read failed: %d\n.", nbytes);
	}
	/*
	else 
	  fprintf(stderr,"Received packet size: %d, ignoring.\n", nbytes);
	*/
      }
 
      //XXX: check for overflow of the data block here?
      //nbytes = fread(buf,1,BUFSIZE,infd);
      //fprintf(stderr,"fread: %d bytes\n",nbytes);
      //status = ipcio_write(&data_block,buf,nbytes);
      //fprintf(stderr,"ipcio_write: %d bytes\n",status);
      
      //sleep(2);
    }
  }
  
  //fclose(infd);
  
  return 0;
}

