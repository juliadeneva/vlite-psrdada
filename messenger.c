#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Connection.h"
#include "def.h"
#include "executor.h"
#include "vlite_xml.h"
#include "alert.h"

#define MSGMAXSIZE 8192

//Multicast ports
#define MULTI_OBSINFO_PORT 53001
#define MULTI_ANTPROP_PORT 53000
#define MULTI_ALERT_PORT 20011
#define MULTI_TEST_PORT 53901

//Multicast group IPs
char testgrp[] = "239.199.3.2";
char obsinfogrp[] = "239.192.3.2";
char antpropgrp[] = "239.192.3.1";
char alertgrp[] = "239.192.2.3";

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
  Connection cr, cw, heimdall;
  fd_set readfds;
  int obsinfosock, antpropsock, alertsock, maxnsock = 0, nbytes;

  ScanInfoDocument D; //multicast message struct
  const ObservationDocument *od;
  const AntPropDocument *ap;
  AlertDocument A; //multicast alert struct

  char scaninfofile[128];
  char msg[MSGMAXSIZE];
  char src[SRCMAXSIZE];
  char from[24]; //ip address of multicast sender

  FILE *sfd;

  //connect to VLA obsinfo multicast
  obsinfosock = openMultiCastSocket(obsinfogrp, MULTI_OBSINFO_PORT);
  if(obsinfosock < 0) {
    fprintf(stderr,"Failed to open Observation multicast; openMultiCastSocket = %d\n",obsinfosock);
    exit(1);
  }
  else {
    printf("Obsinfo socket: %d\n",obsinfosock);
    if(obsinfosock > maxnsock)
      maxnsock = obsinfosock;
  }

  //connect to VLA antprop multicast
  antpropsock = openMultiCastSocket(antpropgrp, MULTI_ANTPROP_PORT);
  if(antpropsock < 0) {
    fprintf(stderr,"Failed to open Antprop multicast; openMultiCastSocket = %d\n",antpropsock);
    exit(1);
  }
  else {
    printf("Antprop socket: %d\n",antpropsock);
    if(antpropsock > maxnsock)
      maxnsock = antpropsock;
  }

  //connect to VLA alert multicast
  alertsock = openMultiCastSocket(alertgrp, MULTI_ALERT_PORT);
  if(alertsock < 0) {
    fprintf(stderr,"Failed to open Alert multicast; openMultiCastSocket = %d\n",alertsock);
    exit(1);
  }
  else {
    printf("Alert socket: %d\n",alertsock);
    if(alertsock > maxnsock)
      maxnsock = alertsock;
  }

  /*
  //connect to testvlite multicast
  obsinfosock = openMultiCastSocket(testgrp, MULTI_TEST_PORT);
  if(obsinfosock < 0) {
    fprintf(stderr,"Failed to open Test multicast; openMultiCastSocket = %d\n",obsinfosock);
    exit(1);
  }
  else {
    printf("Test socket: %d\n",obsinfosock);
    if(obsinfosock > maxnsock)
      maxnsock = obsinfosock;
  }
  */

  fprintf(stderr,"maxnsock: %d\n",maxnsock);

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

    fprintf(stderr,"Waiting for multicast message...\n");

    //Blocking select() on multicast sockets and Heimdall event trigger socket
    //Have to call FS_SET on each socket before calling select()
    FD_ZERO(&readfds);
    FD_SET(obsinfosock, &readfds);
    FD_SET(antpropsock, &readfds);
    FD_SET(alertsock, &readfds);
    select(maxnsock+1,&readfds,NULL,NULL,NULL);
    
    //Obsinfo socket
    if(FD_ISSET(obsinfosock,&readfds)) {
      nbytes = MultiCastReceive(obsinfosock, msg, MSGMAXSIZE, from);
      fprintf(stderr,"Received %d bytes from Obsinfo multicast.\n",nbytes);
      if(nbytes <= 0) {
	fprintf(stderr,"Error on Obsinfo socket, return value = %d\n",nbytes);
	continue;
      }
      
      parseScanInfoDocument(&D, msg);
      printScanInfoDocument(&D);
      printf("Message type: %d = %s\n",D.type,ScanInfoTypeString[D.type]);
      if(D.type == SCANINFO_OBSERVATION) {
	od = &(D.data.observation);
	strcpy(src,od->name);
	//printf("src: %s\n",src);
	sprintf(scaninfofile,"%s.obsinfo.%04d.%04d.txt",od->datasetId,od->scanNo,od->subscanNo);
	sfd = fopen(scaninfofile,"w");
	fprintScanInfoDocument(&D,sfd);
	fclose(sfd);

	/*
	if (strcasecmp(src,"FINISH") == 0) {
	  if (send(cw.svc, cmdstop, strlen(cmdstop), 0) == -1)
	    perror("send");
	}
	else {
	  sprintf(cmdstart,"start %s",src);
	  if (send(cw.svc, cmdstart, strlen(cmdstart), 0) == -1)
	    perror("send");	  
	}
	*/
      }
    }

    //Antprop socket
    if(FD_ISSET(antpropsock,&readfds)) {
      nbytes = MultiCastReceive(antpropsock, msg, MSGMAXSIZE, from);
      fprintf(stderr,"Received %d bytes from Antprop multicast.\n",nbytes);
      if(nbytes <= 0) {
	fprintf(stderr,"Error on Antprop socket, return value = %d\n",nbytes);
	continue;
      }

      parseScanInfoDocument(&D, msg);
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

    //Alert socket
    if(FD_ISSET(alertsock,&readfds)) {
      nbytes = MultiCastReceive(alertsock, msg, MSGMAXSIZE, from);
      fprintf(stderr,"Received %d bytes from Alert multicast.\n",nbytes);
      if(nbytes <= 0) {
	fprintf(stderr,"Error on Alert socket, return value = %d\n",nbytes);
	continue;
      }

      parseAlertDocument(&A, msg);
      printAlertDocument(&A);
      sprintf(scaninfofile,"%f.alert.txt",A.timeStamp);
      sfd = fopen(scaninfofile,"w");
      fprintAlertDocument(&A,sfd);
      fclose(sfd);
      
    }


    /*
    if(FD_ISSET(heimdall.rqst,&readfds)) {
      //read() or recv()--form of message TBD
      //send EVENT to Writer
    }
    */
    
  } //end while

  //no need to close connections, just send quit commands to reader & writer
  //if(send(cw.svc, cmdquit, strlen(cmdquit),0) == -1)
  //perror("send");
  //if(send(cr.svc, cmdquit, strlen(cmdquit),0) == -1)
  //  perror("send");
  
}
