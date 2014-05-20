#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Connection.h"
#include "def.h"
#include "executor.h"
#include "vlite_xml.h"

#define MSGMAXSIZE 32768

char hostname[] = "furby.nrl.navy.mil";
char obsfile[] = "observation.xml";
char antfile[] = "antprop.xml";

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
  Connection cr;
  Connection cw;

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

  //connect to writer; writer is in STATE_STOPPED by default
  if(conn(hostname,WRITER_SERVICE_PORT,&cw) < 0) {
    fprintf(stderr,"Messenger: could not connect to Writer.\n");
    exit(1);
  }
    
  //connect to reader; reader is in STATE_STARTED by default
  /*
  if(conn(hostname,READER_SERVICE_PORT,&cr) < 0 ) {
    fprintf(stderr,"Messenger: could not connect to Reader.\n");
    exit(1);
  }
  */

  //XXX: connect to VLA obsinfo multicast
  //XXX: connect to VLA antprop multicast

  while(1) {
    /* From Walter: The antprop message comes whenever a new scheduling block starts and at each UT midnight (where EOP values might change). Observation document happens before each new scan and a FINISH document at the end of a scheduling block. At the beginning of the scheduling block the antprop document always precedes the observation document.
     */

    //select() on multicast sockets
    //read waiting stuff from antprop socket & save it to file
    //read waiting stuff from obsinfo socket & send commands to Reader/Writer
    //blocking read on antprop socket
    //Would it be easier to have a separate thread/program deal with the antprop socket since none of its traffic is necessary for Reader/Writer?

    //parse antprop message and write it to file
    parseScanInfoDocument(&D, antmsg);
    printScanInfoDocument(&D);
    ap = &(D.data.antProp);
    sprintf(scaninfofile,"%s.antprop.txt",ap->datasetId);
    sfd = fopen(scaninfofile,"w");
    fprintScanInfoDocument(&D,sfd);
    fclose(sfd);

    //parse obsinfo message, write it to file, extract src (XXX: extract other parameters and send them to Heimdall)
    parseScanInfoDocument(&D, obsmsg);
    printScanInfoDocument(&D);
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
