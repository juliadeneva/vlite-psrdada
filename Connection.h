#include <sys/socket.h>
#include <arpa/inet.h>	/* for inet_ntoa */
#include <netinet/in.h>

#define MAXINBUFSIZE 128
#define MAXHOSTNAME 128

#define WRITER_SERVICE_PORT  30001
#define READER_SERVICE_PORT  40001

typedef struct {
  int svc;        /* listening socket providing service */
  int rqst;       /* socket accepting the request */
  socklen_t alen;       /* length of address structure */
  struct sockaddr_in my_addr;    
  struct sockaddr_in rem_addr;  
  int sockoptval;
  char hostname[MAXHOSTNAME]; /* host name, for debugging */  
  char buf[MAXINBUFSIZE]; /* incoming messages go here */
  int port; 
  int isconnected; /* 0 if not connected, >0 if connected*/

} Connection;
