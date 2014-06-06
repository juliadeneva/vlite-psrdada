#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Connection.h"
#include "def.h"
#include "executor.h"
#include "vlite_xml.h"

#define MSGMAXSIZE 8192

char hostname[] = "furby.nrl.navy.mil";
char obsfile[] = "Observation.xml";
char antfile[] = "SampleAntennaProperties.xml";

void usage ()
{
  fprintf(stdout,"Usage: messenger\n");
  //XXX: add options to connect to writers/readers at certain ports?
}


int main(int argc, char** argv)
{
  char cmdstop[] = "stop";
  char cmdquit[] = "quit";
  char cmdstart[32]; //this will be of the form 'start <src>'

  //XXX: in final version, there will be an array of readers and an array of writers, so initial connection attempts as well as each command send will loop over those. (How to get port numbers for all? Specify on command line?)  
  Connection cr, cw, multiobs, multiantprop, heimdall;
  fd_set readfds;

  ScanInfoDocument D; //multicast message struct
  const ObservationDocument *od;
  const AntPropDocument *ap;
  
  char scaninfofile[128];
  char obsmsg[MSGMAXSIZE];
  char antmsg[MSGMAXSIZE];
  char src[SRCMAXSIZE];

  /* For debugging with files */
  FILE *sfd;
  int rc;
  long off_end;
  size_t fsz;

  sfd = fopen(obsfile,"r");
  rc = fseek(sfd, 0, SEEK_END);
  off_end = ftell(sfd);
  fsz = (size_t)off_end;
  //printf("%s size: %d\n",obsfile,fsz);

  rewind(sfd);
  if(fsz != fread(obsmsg, 1, fsz, sfd) ) {
        return -1;
  }  
  fclose(sfd);

  sfd = fopen(antfile,"r");
  rc = fseek(sfd, 0, SEEK_END);
  off_end = ftell(sfd);
  fsz = (size_t)off_end;
  //printf("%s size: %d\n",antfile,fsz);

  rewind(sfd);
  if(fsz != fread(antmsg, 1, fsz, sfd) ) {
        return -1;
  }  
  fclose(sfd);
  /* End debugging with files */

  //connect to VLA obsinfo multicast; XXX: make conn() work with IP address if ulticast host has no name
  //obsinfo host IP: 239.192.3.2, antprop host IP: 239.192.3.1
  /*
  if(conn(hostname,MULTI_OBSINFO_PORT,&multiobs) < 0 ) {
  fprintf(stderr,"Messenger: could not connect to Obsinfo multicast.\n");
  exit(1);
  }
  */

  //connect to VLA antprop multicast
  /*
  if(conn(hostname,MULTI_ANTPROP_PORT,&multiantprop) < 0 ) {
    fprintf(stderr,"Messenger: could not connect to Antprop multicast.\n");
    exit(1);
  }
  */

  //XXX: open a listening socket for Heimdall, wait for connection
  
  //connect to writer; writer is in STATE_STOPPED by default
  /*
  if(conn(hostname,WRITER_SERVICE_PORT,&cw) < 0) {
    fprintf(stderr,"Messenger: could not connect to Writer.\n");
    exit(1);
  }
  */

  //connect to reader; reader is in STATE_STARTED by default
  /*
  if(conn(hostname,READER_SERVICE_PORT,&cr) < 0 ) {
    fprintf(stderr,"Messenger: could not connect to Reader.\n");
    exit(1);
  }
  */

  while(1) {
    /* From Walter: The antprop message comes whenever a new scheduling block starts and at each UT midnight (where EOP values might change). Observation document happens before each new scan and a FINISH document at the end of a scheduling block. At the beginning of the scheduling block the antprop document always precedes the observation document.
     */

    //Blocking select() on multicast sockets and Heimdall event trigger socket
    //Have to call FS_SET on each socket before calling select()
    /*
    FD_ZERO(&readfds);
    FD_SET(multiobs.rqst, &readfds);
    FD_SET(multiantprop.rqst, &readfds);
    select(multiantprop.rqst+1,&readfds,NULL,NULL,NULL);
    
    //Types of packets on Obsinfo socket: Obsinfo or Tcal
    if(FD_ISSET(multiobs.rqst,&readfds)) {
      //read() or recv() here
      parseScanInfoDocument(&D, obsmsg);
      printScanInfoDocument(&D);
      printf("Message type: %d = %s\n",D.type,ScanInfoTypeString[D.type]);
      if(D.type == SCANINFO_OBSERVATION) {
	od = &(D.data.observation);
	strcpy(src,od->name);
	//printf("src: %s\n",src);
	sprintf(scaninfofile,"%s.obsinfo.txt",od->datasetId);
	sfd = fopen(scaninfofile,"w");
	fprintScanInfoDocument(&D,sfd);
	fclose(sfd);
	
	if (strcasecmp(src,"FINISH") == 0) {
	  if (send(cw.svc, cmdstop, strlen(cmdstop), 0) == -1)
	    perror("send");
	}
	else {
	  sprintf(cmdstart,"start %s",src);
	  if (send(cw.svc, cmdstart, strlen(cmdstart), 0) == -1)
	    perror("send");	  
	}
      }
    }

    //Type of packets on Antprop socket: Antprop, CorrSubarrayTable, VlaCorrelatorMode
    if(FD_ISSET(multiantprop.rqst,&readfds)) {
      //read() or recv() here
      parseScanInfoDocument(&D, antmsg);
      printScanInfoDocument(&D);
      printf("Message type: %d = %s\n",D.type,ScanInfoTypeString[D.type]);
      if(D.type == SCANINFO_ANTPROP) {
	ap = &(D.data.antProp);
	sprintf(scaninfofile,"%s.antprop.txt",ap->datasetId);
	sfd = fopen(scaninfofile,"w");
	fprintScanInfoDocument(&D,sfd);
	fclose(sfd);
      }
    }

    if(FD_ISSET(heimdall.rqst,&readfds)) {
      //read() or recv()--form of message TBD
      //send EVENT to Writer
    }

    */

    //parse antprop message and write it to file
    parseScanInfoDocument(&D, antmsg);
    printScanInfoDocument(&D);
    printf("Message type: %d = %s\n",D.type,ScanInfoTypeString[D.type]);
    ap = &(D.data.antProp);
    sprintf(scaninfofile,"%s.antprop.txt",ap->datasetId);
    sfd = fopen(scaninfofile,"w");
    fprintScanInfoDocument(&D,sfd);
    fclose(sfd);

    //parse obsinfo message, write it to file, extract src (XXX: extract other parameters and send them to Heimdall)
    parseScanInfoDocument(&D, obsmsg);
    printScanInfoDocument(&D);
    printf("Message type: %d = %s\n",D.type,ScanInfoTypeString[D.type]);
    od = &(D.data.observation);
    strcpy(src,od->name);
    //printf("src: %s\n",src);
    sprintf(scaninfofile,"%s.obsinfo.txt",od->datasetId);
    sfd = fopen(scaninfofile,"w");
    fprintScanInfoDocument(&D,sfd);
    fclose(sfd);
    
    //If src name is FINISH, send CMD_STOP to Writer; Reader will stop when it finds the EOD written by Writer to the ring buffer
    if (strcasecmp(src,"FINISH") == 0) {
      if (send(cw.svc, cmdstop, strlen(cmdstop), 0) == -1)
	perror("send");
    }
    else {
      sprintf(cmdstart,"start %s",src);
      if (send(cw.svc, cmdstart, strlen(cmdstart), 0) == -1)
	perror("send");
      
      sleep(5);
    }

    break;
    
  } //end while

  //no need to close connections, just send quit commands to reader & writer
  if(send(cw.svc, cmdquit, strlen(cmdquit),0) == -1)
    perror("send");
  //if(send(cr.svc, cmdquit, strlen(cmdquit),0) == -1)
  //  perror("send");
  
}
