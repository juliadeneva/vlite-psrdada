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

//How many Reader/Writer pairs are there
#define NRWPAIRS 4
#define HOST0 2 //read data from difxN hosts where N >= HOST0

void usage ()
{
  fprintf(stdout,"Usage: messenger\n");
  //XXX: add options to connect to writers/readers at certain ports?
}


int main(int argc, char** argv)
{
  char cmdstop[] = "stop";
  char cmdevent[] = "event";
  char cmdquit[] = "quit";
  char cmdstart[32]; //this will be of the form 'start <src>'

  //XXX: in final version, there will be an array of readers and an array of writers, so initial connection attempts as well as each command send will loop over those. (How to get port numbers for all? Specify on command line?)  
  Connection cr[NRWPAIRS];
  Connection cw[NRWPAIRS];
  char hostname[MAXHOSTNAME]; // = "vlite-difx1.evla.nrao.edu";
  fd_set readfds;
  int obsinfosock, antpropsock, alertsock, maxnsock = 0, nbytes, ii, nhost = HOST0-1;

  ScanInfoDocument D; //multicast message struct
  const ObservationDocument *od;
  const AntPropDocument *ap;
  //AlertDocument A; 

  char scaninfofile[128];
  char msg[MSGMAXSIZE];
  char src[SRCMAXSIZE];
  char from[24]; //ip address of multicast sender

  FILE *sfd;

  //Initialize Connections and connect to Readers/Writers: two pairs per difx host
  for(ii=0; ii<NRWPAIRS; ii++) {
    if(ii%2 == 0) {
      nhost++;
      cr[ii].port = READER_SERVICE_PORT;
      cw[ii].port = WRITER_SERVICE_PORT;
    }
    else {
      cr[ii].port = READER_SERVICE_PORT + 1;
      cw[ii].port = WRITER_SERVICE_PORT + 1;
    }
    
    sprintf(cr[ii].hostname,"vlite-difx%d.evla.nrao.edu",nhost);
    sprintf(cw[ii].hostname,"vlite-difx%d.evla.nrao.edu",nhost);
    cr[ii].isconnected = 0;
    cw[ii].isconnected = 0;

    printf("Reader on %s, port %d\n", cr[ii].hostname, cr[ii].port);
    printf("Writer on %s, port %d\n", cw[ii].hostname, cw[ii].port);

    if(conn(&cw[ii]) < 0) {
      fprintf(stderr,"Messenger: could not connect to Writer on %s port %d\n", cw[ii].hostname, cw[ii].port);
      exit(1);
    }
    else
      cw[ii].isconnected = 1;
    
    if(conn(&cr[ii]) < 0 ) {
      fprintf(stderr,"Messenger: could not connect to Reader on %s port %d\n", cr[ii].hostname, cr[ii].port);
      exit(1);
    }
    else
      cr[ii].isconnected = 1;
  }

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

  /*
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
  */

  while(1) {
    /* From Walter: The antprop message comes whenever a new scheduling block starts and at each UT midnight (where EOP values might change). Observation document happens before each new scan and a FINISH document at the end of a scheduling block. At the beginning of the scheduling block the antprop document always precedes the observation document.
     */

    //Blocking select() on multicast sockets and Heimdall event trigger socket
    //Have to call FS_SET on each socket before calling select()
    FD_ZERO(&readfds);
    FD_SET(obsinfosock, &readfds);
    FD_SET(antpropsock, &readfds);
    //FD_SET(alertsock, &readfds);

    printf("Waiting for multicast messages...\n");
    select(maxnsock+1,&readfds,NULL,NULL,NULL);
    
    //Obsinfo socket
    if(FD_ISSET(obsinfosock,&readfds)) {
      nbytes = MultiCastReceive(obsinfosock, msg, MSGMAXSIZE, from);
      //fprintf(stderr,"Received %d bytes from Obsinfo multicast.\n",nbytes);
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


	// TO ADD: Check that Writers are still connected before sending; change their isonnected elements if they are not
	if (strcasecmp(src,"FINISH") == 0) {
	  for(ii=0; ii<NRWPAIRS; ii++) {
	    if (send(cw[ii].svc, cmdstop, strlen(cmdstop)+1, 0) == -1)
	      perror("send");
	  }
	}
	else {
	//else if(strstr(src,"B0329+54") != NULL || strstr(src,"B0531+21") != NULL || strstr(src,"B0950+08") != NULL || strstr(src,"B0833-45") != NULL || strstr(src,"B1749-28") != NULL || strstr(src,"J0437-4715") != NULL || strstr(src,"B1642-03") != NULL || strstr(src,"B1641-45") != NULL || strstr(src,"J0341+5711") != NULL) {
	  sprintf(cmdstart,"start %s",src);
	  

	  // TO ADD: Check that Readers/Writers are still connected before sending; change their isonnected elements if they are not
    for(ii=0; ii<NRWPAIRS; ii++) {
	    if (send(cr[ii].svc, cmdstart, strlen(cmdstart)+1, 0) == -1)
	      perror("send");
	    if (send(cw[ii].svc, cmdstart, strlen(cmdstart)+1, 0) == -1)
	      perror("send");
	  }

	  sleep(60); //let the ring buffer fill
	  
	  for(ii=0; ii<NRWPAIRS; ii++) {
	    if (send(cw[ii].svc, cmdevent, strlen(cmdevent)+1, 0) == -1)
	      perror("send");
	  }


	  //sleep(2); 

	  /*
	  for(ii=0; ii<NRWPAIRS; ii++) {
	    if (send(cw[ii].svc, cmdstop, strlen(cmdstart)+1, 0) == -1)
	      perror("send");
	    if (send(cr[ii].svc, cmdstop, strlen(cmdstart)+1, 0) == -1)
	      perror("send");
	  }
	  */
	}

      }
    }

    //Antprop socket
    if(FD_ISSET(antpropsock,&readfds)) {
      nbytes = MultiCastReceive(antpropsock, msg, MSGMAXSIZE, from);
      //fprintf(stderr,"Received %d bytes from Antprop multicast.\n",nbytes);
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

    /*
    //Alert socket
    if(FD_ISSET(alertsock,&readfds)) {
      nbytes = MultiCastReceive(alertsock, msg, MSGMAXSIZE, from);
      //fprintf(stderr,"Received %d bytes from Alert multicast.\n",nbytes);
      if(nbytes <= 0) {
	fprintf(stderr,"Error on Alert socket, return value = %d\n",nbytes);
	continue;
      }

      parseAlertDocument(&A, msg);
      //if((strcmp(A.monitorName,"ELPosError") == 0 || strcmp(A.monitorName,"AZPosError") == 0) && A.alertState == 1) {
      if(strcmp(A.monitorName,"ELPosError") == 0 || strcmp(A.monitorName,"AZPosError") == 0) {
	printAlertDocument(&A);
	printf("alertState = %d\n", A.alertState);
	sprintf(scaninfofile,"%f.alert.txt",A.timeStamp);
	sfd = fopen(scaninfofile,"w");
	fprintAlertDocument(&A,sfd);
	fclose(sfd);
      }
    }
    */

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

  //close multicast sockets
  
}
