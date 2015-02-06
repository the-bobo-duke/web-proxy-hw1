/*
 * proxy.c - Web proxy for COMPSCI 512
 *
 */

/*
* count, i, hp, haddrp, tid, args
*/

#include <stdio.h>
#include "csapp.h"
#include <pthread.h>

#define   FILTER_FILE   "proxy.filter"
#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	  "proxy.debug"
#define   MAX_HEADER    8192
// set max size of 8192 bytes for our HTTP headers based on Apache default limit


/*============================================================
 * function declarations
 *============================================================*/

int  find_target_address(char * uri,
			 char * target_address,
			 char * path,
			 int  * port);


void  format_log_entry(char * logstring,
		       int sock,
		       char * uri,
		       int size);
		       
void *forwarder(void* args);
void *webTalk(void* args);
void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort);
void ignore();

int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/* main function for the proxy program */

int main(int argc, char *argv[])
{
  int count = 0;
  int listenfd, connfd, clientlen, optval, serverPort, i;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;
  sigset_t sig_pipe; 
  pthread_t tid;
  int *args;
  
  if (argc < 2) {
    printf("Usage: %s port [debug] [webServerPort]\n", argv[0]);
    exit(1);
  }
  if(argc == 4)
    serverPort = atoi(argv[3]);
  else
    serverPort = 80;
  
  Signal(SIGPIPE, ignore);
  
  if(sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
    unix_error("creating sig_pipe set failed");
  if(sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
    unix_error("sigprocmask failed");
  
  proxyPort = atoi(argv[1]);

  if(argc > 2)
    debug = atoi(argv[2]);
  else
    debug = 0;


  /* start listening on proxy port */
/* ================================================================
* START LISTENING ON PROXY PORT
* =================================================================
*/
  listenfd = Open_listenfd(proxyPort);

  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  if(debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

  logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);    


  /* if writing to log files, force each thread to grab a lock before writing
     to the files */
  
  pthread_mutex_init(&mutex, NULL);
  while(1) {
    clientlen = sizeof(clientaddr);

    /* accept a new connection from a client here */

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    /* you have to write the code to process this new client request */

    /* ssize_t Read(int fd, void *buf, size_t count); */
    // just to show that reading works
    /*
    char * buf = malloc(2048);
    
    if (connfd > 0){
      Read(connfd, buf, 2048);
      fprintf(stderr, "\nHere's what comes out: \n%s", buf);
    }
    */
    
    /* create a new thread (or two) to process the new connection */
    
    int newargv[] = {connfd, serverPort};
    Pthread_create(&tid, NULL, webTalk, newargv);

    /*
    if(Pthread_create(&tid, NULL, threadStart, connfd)){
      fprintf(stderr, "\nError making thread\n");
      return EXIT_FAILURE;
    }
    */
  }
  
  if(debug) Close(debugfd);
  Close(logfd);
  pthread_mutex_destroy(&mutex);
  
  return 0;
}

/* a possibly handy function that we provide, fully written */

/* ================================================================
* parseAddress FUNCTION
* =================================================================
*/

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char *point1;
        char *point2;
        char *saveptr;

	if(strstr(url, "http://"))
		url = &(url[7]);
	*file = strchr(url, '/');
	
	strcpy(host, url);

	/* first time strtok_r is called, returns pointer to host */
	/* strtok_r (and strtok) destroy the string that is tokenized */

	/* get rid of everything after the first / */

	strtok_r(host, "/", &saveptr);

	/* now look to see if we have a colon */

	point1 = strchr(host, ':');
	if(!point1) {
		*serverPort = 80;
		return;
	}
	
	/* we do have a colon, so get the host part out */
	strtok_r(host, ":", &saveptr);

	/* now get the part after the : */
	*serverPort = atoi(strtok_r(NULL, "/",&saveptr));
}

/* ================================================================
* webTalk FUNCTION - PROCESS GET REQUESTS, PASS CONNECT REQUESTS
* =================================================================
*/

/* this is the function that I spawn as a thread when a new
   connection is accepted */

/* you have to write a lot of it */

/*
* Sets up thread 
* Reads in browser requests (RIO)
* Calls parseAddres
* Calls webTalk
* Closes connfd
*/

void *webTalk(void* args)
{
  int numBytes, lineNum, serverfd, clientfd, serverPort;
  int tries;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE], logString[MAXLINE];
  char *token, *cmd, *version, *file, *saveptr;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");

  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  int * serverPort_ptr = &serverPort;
  char * file_ptr = &file;
  char * uri;
  //free(args);
  
  Rio_readinitb(&client, clientfd);
  size_t rio_return = Rio_readlineb(&client, buf1, MAXLINE);

  // Determine protocol (CONNECT or GET)
  fprintf(stderr, "\nbuf1: %s\n", buf1);
  
  if (buf1[0] == 'G'){
    fprintf(stderr, "\nWe should process a GET request\n");
    // need to isolate the "1" in "HTTP/1.1" so we can change it to 0
    fprintf(stderr, "\nthis should say 1: %c\n", buf1[rio_return-3]);
    // -3 because: \n char, terminating null char, and rio_return is # bytes read
    // but array indecies start at 0 not 1

    //copy buf1 to buf2 so we can later recover full HTTP Header
    memcpy(&buf2, buf1, MAXLINE);

    // extract uri for use in parseAddress
    if (rio_return > 0){
    uri = strchr(buf1, 'h');
    strtok_r(uri, " ", &saveptr);
    fprintf(stderr, "\nuri is: %s\n", uri);
    }
    // call parseAddress to get hostname back
    //void parseAddress(char* url, char* host, char** file, int* serverPort)
    
    parseAddress(uri, host, file_ptr, serverPort_ptr);
    /*
    fprintf(stderr, "\n host is: %s\n", host);
    fprintf(stderr, "\n file path is: %s\n", file);
    fprintf(stderr, "\n serverPort should still be 80: %d\n", serverPort);
    */

// GET: open connection to webserver (try several times, if necessary)

    serverfd = Socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0){
      fprintf(stderr, "Error opening socket for serverfd");
      return EXIT_FAILURE;
    }

    struct hostent *server_he = Gethostbyname(host);
    struct sockaddr_in server_sa_in;

    memcpy(&server_sa_in.sin_addr, server_he->h_addr_list[0], server_he->h_length);
    server_sa_in.sin_family = AF_INET;
    server_sa_in.sin_port = htons(serverPort);

    /*
    fprintf(stderr, "\nvalue of Connect call is: %d\n", 
      (Connect(serverfd, (struct sockaddr *)&server_sa_in, sizeof(server_sa_in)) < 0));
    */
    
    if (Connect(serverfd, (struct sockaddr *)&server_sa_in, sizeof(server_sa_in)) < 0){
      return EXIT_FAILURE;
    }
    else {
      fprintf(stderr, "\n TCP success\n");
    }

    /* GET: Transfer first header to webserver */
    // initialize some string to hold the header
    // write the first two lines (GET and Host: ) manually
    // then iterate through the currently existing HTTP header req
    // and add those lines to the string
    // but look for a Proxy-Connection or a Connection
    // and change those to "Proxy-Connection: close"
    // end of the header is \r\n
    // set max size of 8192 bytes for our headers based on Apache default limit

    // buf2 has full GET ... HTTP/1.1 line in it. How to get rest of header?
    // then need to modify header to suppress keep-alive

    fprintf(stderr, "buf 2 before while: %s\n", buf2);
    fprintf(stderr, "buf3 before while: %s\n", buf3);
    
    int break_me = 0;

    while (break_me == 0){
      Rio_readlineb(&client, buf3, MAXLINE);
      fprintf(stderr, "value of break_me: %d\n", break_me);
      if (buf3[0] == '\r' && buf3[1] == '\n'){
        ++break_me;
      }
      strcat(buf2, buf3); 
    }
    fprintf(stderr, "\n======================AFTER WHILE LOOP\n");
    fprintf(stderr, "buf 3: %s\n===========", buf3);
    fprintf(stderr, "buf 2: \n%s", buf2);
    fprintf(stderr, "======\n");




    /*
    size_t rio_return2 = Rio_readlineb(&client, buf3, MAXLINE);
    strcat(buf2, buf3);
    fprintf(stderr, "buf 3 before while: %s\n", buf3);
    fprintf(stderr, "buf 2 before while: %s\n", buf2);
    fprintf(stderr, "value of rio_return2 o/s while: %d\n", rio_return2);
    while (break_me != 0){
      rio_return2 = Rio_readlineb(&client, buf3, MAXLINE);
    
      fprintf(stderr, "\nbuf 3: %s\n", buf3);
      strcat(buf2, buf3);
      fprintf(stderr, "buf 2: %s\n", buf2);

      if (buf3 == '\r\n')
        fprintf(stderr, "inside if\n");
        break_me = 0;
    }
    */

    /*
    //char hdr_to_send[MAX_HEADER];
    //char first_lines[];
    char str1 = "GET: http://";
    char str2 = " HTTP/1.0\nHost: ";
    fprintf(stderr, "\nsize of str1: %lu", sizeof(str1));
    fprintf(stderr,  " size of str2: %lu", sizeof(str2)); 
    fprintf(stderr, " size of host string: %lu\n", sizeof(host));
    //strcat(first_lines, str1);
    //strcat(first_lines, host);
    //strcat(first_lines, str2);
    //strcat(first_lines, host);
     
    //fprintf(stderr, "%s", first_lines);
    */

    // GET: Transfer remainder of the request

    // GET: now receive the response


  }
  else if (buf1[0] == 'C'){
    fprintf(stderr, "\nWe should process a CONNECT request\n");

    // CONNECT: call a different function, securetalk, for HTTPS

  }

}


/* this function handles the two-way encrypted data transferred in
   an HTTPS connection */

void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort)
{
  int serverfd, numBytes1, numBytes2;
  int tries;
  rio_t server;
  char buf1[MAXLINE], buf2[MAXLINE];
  pthread_t tid;
  int *args;

  if (serverPort == proxyPort)
    serverPort = 443;
  
  /* Open connecton to webserver */
  /* clientfd is browser */
  /* serverfd is server */
  
  
  /* let the client know we've connected to the server */

  /* spawn a thread to pass bytes from origin server through to client */

  /* now pass bytes from client to server */

}

/* this function is for passing bytes from origin server to client */

void *forwarder(void* args)
{
  int numBytes, lineNum, serverfd, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0];
  serverfd = ((int*)args)[1];
  free(args);

  while(1) {
    
    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */
    
  }
}


void ignore()
{
	;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int  find_target_address(char * uri, char * target_address, char * path,
                         int  * port)

{
  //  printf("uri: %s\n",uri);
  

    if (strncasecmp(uri, "http://", 7) == 0) {
	char * hostbegin, * hostend, *pathbegin;
	int    len;
       
	/* find the target address */
	hostbegin = uri+7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	if (hostend == NULL){
	  hostend = hostbegin + strlen(hostbegin);
	}
	
	len = hostend - hostbegin;

	strncpy(target_address, hostbegin, len);
	target_address[len] = '\0';

	/* find the port number */
	if (*hostend == ':')   *port = atoi(hostend+1);

	/* find the path */

	pathbegin = strchr(hostbegin, '/');

	if (pathbegin == NULL) {
	  path[0] = '\0';
	  
	}
	else {
	  pathbegin++;	
	  strcpy(path, pathbegin);
	}
	return 0;
    }
    target_address[0] = '\0';
    return -1;
}



/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char * logstring, int sock, char * uri, int size)
{
    time_t  now;
    char    buffer[MAXLINE];
    struct  sockaddr_in addr;
    unsigned  long  host;
    unsigned  char a, b, c, d;
    int    len = sizeof(addr);

    now = time(NULL);
    strftime(buffer, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (getpeername(sock, (struct sockaddr *) & addr, &len)) {
      /* something went wrong writing log entry */
      printf("getpeername failed\n");
      return;
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a,b,c,d, uri, size);
}
