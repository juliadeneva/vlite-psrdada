#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "def.h"
#include "vdifio.h"

void usage ()
{
  fprintf(stdout,"Usage: db_unwrap [ring buffer dump file]\n");
}

int main(int argc, char** argv)
{
  char outfilename[128];
  FILE *infd, *outfd;
  long framediff, threadid, framenum[2], framenumtmp, framecount = 0;
  char buf[VDIF_PKT_SIZE];
  long framedisc; //after what framecount is the discontinuity
  int numdisc[2]; // how many discontinuities for each threadid
  
  if(argc != 2) {
    usage();
    return 0;
  }

  if((infd = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr,"db_unwrap: Could not open input file %s \n", argv[1]);
    exit(1);
  }

  //Initialize 
  framenum[0] = -1;
  framenum[1] = -1;
  threadid = -1;
  framedisc = -1;
  numdisc[0] = 0;
  numdisc[1] = 0;

  //Go through the file, check for skipped frames and find the ring buffer wrap discontinuity
  while(!feof(infd)) {
    fread(buf,VDIF_PKT_SIZE,1,infd);
    
    framenumtmp = getVDIFFrameNumber((vdif_header *)buf);
    threadid = getVDIFThreadID((vdif_header *)buf);
    framediff = framenumtmp - framenum[threadid];

    if(framediff != -25599 && abs(framediff) > 1 && framenum[threadid] != -1) {
      fprintf(stderr,"At framecount=%ld, FRAME SKIP FROM %d to %d (THREAD %d)\n",framecount,framenum[threadid],framenumtmp,threadid);
      numdisc[threadid]++;
      
      if(framedisc == -1)
	framedisc = framecount;
    }
    else if(framenum[threadid] == -1) {
      fprintf(stderr,"db_unwrap: Thread %d First frame: %d\n",threadid,framenumtmp);
    }
    
    framenum[threadid] = framenumtmp;
    framecount++;
  }

  printf("Total: framecount = %ld\n",framecount);
  printf("framedisc = %ld\n",framedisc);
  printf("Numdisc[0],[1] = %d, %d\n",numdisc[0],numdisc[1]);

  if(numdisc[0] > 1 || numdisc[1] > 1) {
    fprintf(stderr,"File %s has more than one discontinuity per polarization; most likely corrupted.\n", argv[1]);
    exit(1);
  }

  if(numdisc[1] == 0 && numdisc[1] == 0) {
    fprintf(stderr,"File %s has no discontinuities, nothing to be done.\n", argv[1]);
    return 0;
  }
  
  //Open output file
  sprintf(outfilename,"%s.uw",argv[1]);
  if((outfd = fopen(outfilename, "w")) == NULL) {
    fprintf(stderr,"db_unwrap: Could not open output file %s \n", outfilename);
    exit(1);
  }

  //Copy data from framedisc onwards into output file
  fseek(infd,framedisc*VDIF_PKT_SIZE,SEEK_SET);
  while(!feof(infd)) {
    fread(buf,VDIF_PKT_SIZE,1,infd);
    fwrite(buf,VDIF_PKT_SIZE,1,outfd);
  }

  //Copy data from start of input file up to framedisc
  fseek(infd,0,SEEK_SET);
  framecount = 0;
  while(framecount < framedisc-1 && !feof(infd)) {
    fread(buf,VDIF_PKT_SIZE,1,infd);
    fwrite(buf,VDIF_PKT_SIZE,1,outfd);
    framecount++;
  }

  fclose(infd);
  fclose(outfd);
  return 0;
}
