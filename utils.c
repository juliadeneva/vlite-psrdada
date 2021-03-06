#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>	/* for inet_ntoa */
#include <netinet/in.h>
#include <netdb.h>      /* for gethostbyname() */
#include <sys/errno.h>   /* defines ERESTART, EINTR */
//#include <sys/wait.h>    /* defines WNOHANG, for wait() */
//#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>

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
  c->alen = sizeof(c->rem_addr);     /* length of address */
  
  for (;;) {
    while ((c->rqst = accept(c->svc,(struct sockaddr *)&(c->rem_addr), &(c->alen))) < 0) {
      /* we may break out of accept if the system call */
      /* was interrupted. In this case, loop back and */
      /* try again */
      if ((errno != ECHILD) && (errno != ERESTART) && (errno != EINTR)) {
	perror("accept failed");
	return -1;
      }
    }
    
    printf("received a connection from: %s port %d\n",
	   inet_ntoa(c->rem_addr.sin_addr), ntohs(c->rem_addr.sin_port));

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
  int nbytes, ii;
  char tmp;

  printf("in wait_for_cmd\n");

  nbytes = read(c->rqst,c->buf,MAXINBUFSIZE);
  c->buf[nbytes] = '\0';
  printf("wait_for_cmd: read %d bytes: %.*s\n",nbytes,nbytes,c->buf);
  
  if(nbytes == 0) {
    fprintf(stderr,"wait_for_cmd: Lost connection with Messenger. Defaulting to CMD_QUIT.\n");
    return CMD_QUIT;
  }

  //Normal operation: one command character received at a time
  if(nbytes == 1) {
    switch(c->buf[0]) {
    case CMD_START: return CMD_START;
    case CMD_STOP: return CMD_STOP;
    case CMD_QUIT: return CMD_QUIT;
    case CMD_EVENT: return CMD_EVENT;
    default: {
      printf("wait_for_cmd: Unrecognized command %c, defaulting to CMD_NONE.\n",c->buf[ii]);
      return CMD_NONE;
    }
    }
  } 
  //Backlogged commands: return the most recent non-event command.
  else {
    for(ii=nbytes-1; ii>=0; ii--) {
      switch(c->buf[ii]) {
      case CMD_START: return CMD_START;
      case CMD_STOP: return CMD_STOP;
      case CMD_QUIT: return CMD_QUIT;
      case CMD_EVENT: continue;
	//case '\0': continue;
      default: {
	printf("wait_for_cmd: Unrecognized command %c, defaulting to CMD_NONE.\n",c->buf[ii]);
	return CMD_NONE;
      }
      }
    }
    
    //We get here if only CMD_EVENTs are backlogged; ignore them.
    return CMD_NONE;
  }
}

/*
int wait_for_cmd(Connection* c, char *src)
{
  int nbytes, n;
  char *tmp, *end;

  printf("in wait_for_cmd\n");

  //while(1) {

    //read from socket (blocking); is it better to use recv()?
    nbytes = read(c->rqst,c->buf,MAXINBUFSIZE);
    printf("wait_for_cmd: read %d bytes: %.*s\n",nbytes,nbytes,c->buf);

    if(nbytes == 0) {
      fprintf(stderr,"wait_for_cmd: Lost connection with Messenger. Defaulting to CMD_QUIT.\n");
      return CMD_QUIT;
    }

    //telnet sends 2 invisible characters; null terminate to get rid of them
    //(won't be necessary in real operation)
    //c->buf[nbytes-2] = '\0';
    
    //terminate string for comparisons (is strncmp better?)
    c->buf[nbytes] = '\0';

    //XXX: If several commands are received as one string, the blocks below will only return the first! Return only the last instead?

    if(strncmp(c->buf,"start",5) == 0) {
      printf("start received\n");
      n = strlen(c->buf);
      fprintf(stderr,"n from strlen: %d\n",n);
      end = c->buf + n;
      tmp = c->buf + 5;

      //fprintf(stderr,"after end/tmp, c->buf: %s\n",c->buf);
      while(isspace(*tmp) && tmp != end) {
	tmp++;
      }
      //fprintf(stderr,"after while, c->buf: %s\n",c->buf);

      strcpy(src,tmp); //assumes received string contains 'start [src]' only
      fprintf(stderr,"source name: %s\n",src);
      //fprintf(stderr,"c->buf: %s\n",c->buf);
      return CMD_START;
    }
    else if (strncmp(c->buf,"stop",4) == 0) {
      printf("stop received\n");
      return CMD_STOP;
    }
    else if (strncmp(c->buf,"quit",4) == 0) {
      printf("quit received\n");
      return CMD_QUIT;
    }
    //have to make sure only Writer can get to this:
    else if (strncmp(c->buf,"event",5) == 0) {
      printf("event received\n");
      return CMD_EVENT;
    }
    else {
      printf("unrecognized command: %s\n",c->buf);
      return CMD_NONE;
    }
    //}
}
*/


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

/* Connect to a specified server and port number */
//Adapted from http://www.cs.rutgers.edu/~pxk/417/notes/sockets/demo-03.html
//int conn(const char *host, int port, Connection *c)
int conn(Connection *c)
{
  struct hostent *hp;	/* host information */
  
  //printf("conn(host=\"%s\", port=\"%d\")\n", c->hostname, c->port);
  
  /* get a tcp/ip socket */
  /* We do this as we did it for the server */
  /* request the Internet address protocol */
  /* and a reliable 2-way byte stream */

  if ((c->svc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("cannot create socket");
    return -1;
  }

  /* bind to an arbitrary return address */
  /* because this is the client side, we don't care about the */
  /* address since no application will connect here  --- */
  /* INADDR_ANY is the IP address and 0 is the socket */
  /* htonl converts a long integer (e.g. address) to a network */
  /* representation (agreed-upon byte ordering */

  memset((char*)&(c->my_addr), 0, sizeof(c->my_addr));  /* 0 out the structure */
  c->my_addr.sin_family = AF_INET;   /* address family */
  c->my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  c->my_addr.sin_port = htons(0);
  //strncpy(c->hostname,host,MAXHOSTNAME);
  
  if (bind(c->svc, (struct sockaddr *)&(c->my_addr), sizeof(c->my_addr)) < 0) {
    perror("bind failed");
    return -1;
  }
  
  /* this part is for debugging only - get the port # that the operating */
  /* system allocated for us. */
  c->alen = sizeof(c->my_addr);
  if (getsockname(c->svc, (struct sockaddr *)&(c->my_addr), &(c->alen)) < 0) {
    perror("getsockname failed");
    return -1;
  }
  //printf("local port number = %d\n", ntohs(c->my_addr.sin_port));
  
  /* fill in the server's address and data */
  /* htons() converts a short integer to a network representation */
  
  memset((char*)&(c->rem_addr), 0, sizeof(c->rem_addr));
  c->rem_addr.sin_family = AF_INET;
  c->rem_addr.sin_port = htons(c->port);
  
  /* look up the address of the server given its name */
  hp = gethostbyname(c->hostname);
  if (!hp) {
    fprintf(stderr, "could not obtain address of %s\n", c->hostname);
    return -1;
  }
  
  /* put the host's address into the server address structure */
  memcpy((void *)&(c->rem_addr.sin_addr), hp->h_addr_list[0], hp->h_length);
  
  /* connect to server */
  if (connect(c->svc, (struct sockaddr *)&(c->rem_addr), sizeof(c->rem_addr)) < 0) {
    perror("connect failed");
    return -1;
  }
  
  return 0;
}

/* Disconnect from service socket */
//Adapted from http://www.cs.rutgers.edu/~pxk/417/notes/sockets/demo-03.html
void disconn(Connection *c)
{
	printf("disconn()\n");
	//shutdown(fd, 2);    /* 2 means future sends & receives are disallowed */
	shutdown(c->svc,2);
}


/* adapted from http://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html */
/* From rawrx.c by Walter Brisken */
int openRawSocket(const char *device, int *deviceIndex)
{
	int s, v;
	struct ifreq ifr;
	struct sockaddr_ll sll;
	int n = 16*1024*1024;

	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(s < 0)
	{
		return -1;
	}

	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));

	strncpy(ifr.ifr_name, device, IFNAMSIZ);
	if(ioctl(s, SIOCGIFINDEX, &ifr) == -1)
	{
		close(s);

		return -3;
	}

	/* is the interface up? */
	ioctl(s, SIOCGIFFLAGS, &ifr);
	if( (ifr.ifr_flags & IFF_UP) == 0)
	{
        	close(s);

		return -5;
	}

	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl (s, SIOCSIFFLAGS, &ifr) == -1)
	{
        	close(s);

		return -6;
	}

	ioctl(s, SIOCGIFINDEX, &ifr);
	
	if(deviceIndex)
	{
		*deviceIndex = ifr.ifr_ifindex;
	}
	
	if(device)
	{
		sll.sll_family = AF_PACKET;
		sll.sll_ifindex = ifr.ifr_ifindex;
		sll.sll_protocol = htons(ETH_P_ALL);

		v = bind(s, (struct sockaddr *)&sll, sizeof(sll));
		if(v < 0)
		{
			close(s);

			return -4;
		}
	}

	return s;
}
