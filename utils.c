#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>	/* for inet_ntoa */
#include <netinet/in.h>
#include <sys/errno.h>   /* defines ERESTART, EINTR */
//#include <sys/wait.h>    /* defines WNOHANG, for wait() */
//#include <unistd.h>

#include "Connection.h"
#include "def.h"
#include "ipcio.h"

//Adapted from http://www.cs.rutgers.edu/~pxk/417/notes/sockets/demo-03.html
int serve(int port, Connection* c)
{	
  int nbytes = -1; //received bytes

  gethostname(c->hostname, MAXHOSTNAME);

  /* get a tcp/ip socket */
  /*   AF_INET is the Internet address (protocol) family  */
  /*   with SOCK_STREAM we ask for a sequenced, reliable, two-way */
  /*   conenction based on byte streams.  With IP, this means that */
  /*   TCP will be used */

  if ((c->svc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("cannot create socket");
    return -1;
  }

  /* we use setsockopt to set SO_REUSEADDR. This allows us */
  /* to reuse the port immediately as soon as the service exits. */
  /* Some operating systems will not allow immediate reuse */
  /* on the chance that some packets may still be en route */
  /* to the port. */

  setsockopt(c->svc, SOL_SOCKET, SO_REUSEADDR, &(c->sockoptval), sizeof(int));

  /* set up our address */
  /* htons converts a short integer into the network representation */
  /* htonl converts a long integer into the network representation */
  /* INADDR_ANY is the special IP address 0.0.0.0 which binds the */
  /* transport endpoint to all IP addresses on the machine. */

  memset((char*)&(c->my_addr), 0, sizeof(c->my_addr));  /* 0 out the structure */
  c->my_addr.sin_family = AF_INET;   /* address family */
  c->my_addr.sin_port = htons(port);
  c->my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind to the address to which the service will be offered */
  if (bind(c->svc, (struct sockaddr *)&(c->my_addr), sizeof(c->my_addr)) < 0) {
    perror("bind failed");
    return -1;
  }

  /* set up the socket for listening with a queue length of 5 */
  if (listen(c->svc, 5) < 0) {
    perror("listen failed");
    return -1;
  }
  
  printf("server started on %s, listening on port %d\n", c->hostname, port);

  /* loop forever - wait for connection requests and perform the service */
  c->alen = sizeof(c->client_addr);     /* length of address */
  
  for (;;) {
    while ((c->rqst = accept(c->svc,(struct sockaddr *)&(c->client_addr), &(c->alen))) < 0) {
      /* we may break out of accept if the system call */
      /* was interrupted. In this case, loop back and */
      /* try again */
      if ((errno != ECHILD) && (errno != ERESTART) && (errno != EINTR)) {
	perror("accept failed");
	return -1;
      }
    }
    
    printf("received a connection from: %s port %d\n",
	   inet_ntoa(c->client_addr.sin_addr), ntohs(c->client_addr.sin_port));

    return 0; 
  }
}

/*
  Input: A connection with an already opened listening socket
  Return: CMD_START, CMD_STOP, CMD_QUIT, or CMD_EVENT
  
  Tries to read from socket until a recognized command string is received 
  (start, stop, quit, event)

  XXX: Check that socket is still open? (how?)
 */
int wait_for_cmd(Connection* c)
{
  int nbytes;
  printf("in wait_for_cmd\n");

  //while(1) {

    //read from socket (blocking)
    nbytes = read(c->rqst,c->buf,MAXINBUFSIZE);
    sleep(2);
    printf("wait_for_cmd: read %d bytes: %s\n",nbytes,c->buf);

    //telnet sends 2 invisible characters; null terminate to get rid of them
    //(won't be necessary in real operation)
    c->buf[nbytes-2] = '\0';

    if(strcmp(c->buf,"start") == 0) {
      printf("start received\n");
      return CMD_START;
    }
    else if (strcmp(c->buf,"stop") == 0) {
      printf("stop received\n");
      return CMD_STOP;
    }
    else if (strcmp(c->buf,"quit") == 0) {
      printf("quit received\n");
      return CMD_QUIT;
    }
    //have to make sure only Writer can get to this:
    else if (strcmp(c->buf,"event") == 0) {
      printf("event received\n");
      return CMD_EVENT;
    }
    else {
      printf("unrecognized command: %s\n",c->buf);
      return CMD_NONE;
    }
    //}
}

/*
  Input: pointer to initialized data block, opened writable file descriptor
  
  Byte copy of the contents of the data block to the file. Does not check 
  where the data discontinuity is in the ring buffer, or which sub-blocks are 
  full or partially full.

  XXX: throw an error if for some reason parts of the data block can't
  be copied? (only possible if someone destroys the data block while this
  function is running)
 */
void event_to_file(const ipcio_t* db, FILE* evfd)
{
  int ii, nbytes;
  
  //basic ring buffer, which we use from the ipcio_t file-like abstraction
  //(cast works only b/c the ipcbuf_t element comes first in the ipcio_t struct)
  ipcbuf_t* buf = (ipcbuf_t *)db; 
  //pointers to sub-blocks
  char** buffer = buf->buffer;
    
  int nbufs = ipcbuf_get_nbufs(buf);
  int bufsz = ipcbuf_get_bufsz(buf);

  printf("event_to_file: nbuf = %d bufsz = %d\n",nbufs,bufsz);
  
  if(nbufs <= 0) {
    fprintf(stderr,"event_to_file: nbufs = %d\n",nbufs);
    exit(1);
  }

  if(bufsz <= 0) {
    fprintf(stderr,"event_to_file: bufsz = %d\n",bufsz);
    exit(1);
  }

  for(ii=0; ii<nbufs; ii++) {
    nbytes = fwrite(buffer[ii],1,bufsz,evfd);
    if(nbytes != bufsz)
      fprintf(stderr,"event_to_file: wrote only %d of %d bytes\n",nbytes,bufsz);
  }
}
