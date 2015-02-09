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
void *forwarder_SSL(void* args);
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
    
    if (connfd > 0){
    int newargv[] = {connfd, serverPort};
    Pthread_create(&tid, NULL, webTalk, newargv);
    }
    /*
    if(Pthread_create(&tid, NULL, threadStart, connfd)){
      fprintf(stderr, "\nError making thread\n");
      return EXIT_FAILURE;
    }
    */
  }
  
  if(debug) Close(debugfd);
  Close(logfd);
  Close(connfd);
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

  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];

  Pthread_detach(pthread_self());

  int tries;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE], logString[MAXLINE];
  char *token, *cmd, *version, *file, *saveptr;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");

  int * serverPort_ptr = &serverPort;
  char * file_ptr = &file;
  char * uri;
  //free(args);
  
  Rio_readinitb(&client, clientfd);
  size_t rio_return = Rio_readlineb(&client, buf1, MAXLINE);

  // Determine protocol (CONNECT or GET)
  fprintf(stderr, "\nBROWSER'S INITIAL REQ: %s\n", buf1);
  
  if (buf1[0] == 'G'){
    //fprintf(stderr, "\nWe should process a GET request\n");
    // need to isolate the "1" in "HTTP/1.1" so we can change it to 0
    //fprintf(stderr, "\nthis should say 1: %c\n", buf1[rio_return-3]);
    // -3 because: \n char, terminating null char, and rio_return is # bytes read
    // but array indecies start at 0 not 1

    //copy buf1 to buf2 so we can later recover full HTTP Header
    memcpy(&buf2, buf1, MAXLINE);

    // extract uri for use in parseAddress
    if (rio_return > 0){
    uri = strchr(buf1, ' ');
    uri++;
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

    serverfd = Open_clientfd(host, serverPort);
    /*
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
    */
    /*
    fprintf(stderr, "\nvalue of Connect call is: %d\n", 
      (Connect(serverfd, (struct sockaddr *)&server_sa_in, sizeof(server_sa_in)) < 0));
    */
    /*
    if (Connect(serverfd, (struct sockaddr *)&server_sa_in, sizeof(server_sa_in)) < 0){
      fprintf(stderr, "\nConnecting to server over TCP failed - in HTTP GET logic\n");
      return EXIT_FAILURE;
    }
    else {
      fprintf(stderr, "\n TCP success (GET logic line: %d)\n", __LINE__);
    }
    */
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
    
    int break_me = 0;

    while (break_me == 0){
      Rio_readlineb(&client, buf3, MAXLINE);
      if (buf3[0] == '\r' && buf3[1] == '\n'){
        ++break_me;
      } 
      strcat(buf2, buf3);
    }

    fprintf(stderr, "buf2 before suppression: \n%s", buf2);

    /*
    int size_to_erase = strlen("HTTP/1.1");
    char httpReplace[size_to_erase];
    strcpy(httpReplace, "HTTP/1.0");    
    
    while ( strstr(buf2, "HTTP/1.1") != NULL ){
      strcpy( (strstr(buf2, "HTTP/1.1")), "HTTP/1.0" );
    }

    fprintf(stderr, "buf2 after http suppression: \n%s\n", buf2);
    */

    int size_to_erase = strlen("keep-alive");
    char closeReplace[size_to_erase];
    strcpy(closeReplace, "close     \0");


    while ( strstr(buf2, "keep-alive") != NULL ){
      strncpy( (strstr(buf2, "keep-alive")), closeReplace, strlen(closeReplace) );
    }
    
    fprintf(stderr, "buf2 after suppression: \n%s", buf2); 


    //write to serverfd to send data to server
    int n;
    n = Rio_writen(serverfd, buf2, MAXLINE);
    if (n < 0){
      fprintf(stderr, "error sending get request to server\n");
    }
    memset(buf3, 0, MAXLINE); //zero out buf3 so we can receive data on it
    

    // GET: Transfer remainder of the request



    // GET: now receive the response
    // void *forwarder(void* args)
    int *args2 = malloc(2 * sizeof(args2));
    args2[0] = clientfd;
    args2[1] = serverfd;
    //int args2[] = {clientfd, serverfd};
    fprintf(stderr, "serverfd in webtalk: %d\n", serverfd);
    pthread_t tid2;
    Pthread_create(&tid2, NULL, &forwarder, args2);
    //forwarder(args2);
 
  }

  else if (buf1[0] == 'C'){
    fprintf(stderr, "\nWe should process a CONNECT request\n");

    memcpy(&buf2, buf1, MAXLINE);
    
    if (rio_return > 0){
    uri = strchr(buf1, ' ');
    uri++;    
    strtok_r(uri, " ", &saveptr);
    fprintf(stderr, "\nuri is: %s\n", uri);
    }
    if (uri != NULL){
    parseAddress(uri, host, file_ptr, serverPort_ptr);
    }

    fprintf(stderr, "parse returns serverPort as: %d\n", serverPort);
    fprintf(stderr, "Host is: %s\n", host);

    if ( (serverfd = Open_clientfd(host, serverPort)) > 0) {
      fprintf(stderr, "TCP success in CONNECT logic line: %d\n", __LINE__);
   
        /* let the client know we've connected to the server */

    char SSL_msg[MAXLINE];
    strcpy(SSL_msg, "HTTP/1.1 200 OK\r\n\r\n");
    Rio_writen(clientfd, SSL_msg, strlen(SSL_msg));

    /* spawn a thread to pass bytes from origin server through to client */
 
    int * args3 = malloc(2 * sizeof(args3));
    args3[0] = clientfd;
    args3[1] = serverfd;
    pthread_t tid3;
    Pthread_create(&tid3, NULL, &forwarder_SSL, args3);
    //forwarder_SSL(args3);
    }

    else {
      fprintf(stderr, "error Open_clientfd for SSL: line: %d\n", __LINE__);
    }

    /* now pass bytes from client to server */

    while ( (numBytes = Rio_readn(clientfd, buf1, MAXLINE)) >=0 ){
      if ( numBytes > 0 ){
        Rio_writen(serverfd, buf1, MAXLINE);
      }
      else if (numBytes == 0){
        //fprintf(stderr, "Client has no more bytes to send to Server (SSL). Line: %d\n", __LINE__);
        //Close(serverfd);
        shutdown(serverfd, 1);
      }
      else if (numBytes < 0){
        fprintf(stderr, "error in passing bytes from Client to Server (SSL). LINE: %d\ncall shutdown(serverfd)?", __LINE__);

      }
    }

    //secureTalk(clientfd, client, host, version, serverPort);

    // CONNECT: call a different function, securetalk, for HTTPS

  }
/*
  else {    // you get an empty request from the browser...
    shutdown(clientfd, 1);
    return 0;
  }*/
return 0;
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
  char host[MAXLINE];
  
  strcpy(host, inHost);

  if (serverPort == proxyPort)
    serverPort = 443;
  
  /* Open connecton to webserver */
  /* clientfd is browser */
  /* serverfd is server */


    /*

    //write to serverfd to send data to server
    int n;
    n = Rio_writen(serverfd, buf2, MAXLINE);
    if (n < 0){
      fprintf(stderr, "error sending get request to server\n");
    }

    */

}

/* this function is for passing bytes from origin server to client */

void *forwarder_SSL(void* args){
  int numBytes, lineNum, serverfd, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0]; 
  serverfd = ((int*)args)[1];

  Pthread_detach(pthread_self());

  while ( (numBytes = Rio_readn(serverfd, buf1, MAXLINE)) >=0 ){
    if ( numBytes > 0 ){
      Rio_writen(clientfd, buf1, MAXLINE);
    }
    else if (numBytes == 0){
      //shutdown(clientfd, 1);
    }
    else if (numBytes < 0){
      fprintf(stderr, "error in forwarder_SSL, LINE: %d\ncall shutdown(serverfd)?", __LINE__);
    }
  }

  shutdown(clientfd, 1);
  //shutdown(serverfd, 1);
  return 0;

}

void *forwarder(void* args)
{
  int numBytes, lineNum, serverfd, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0];
  serverfd = ((int*)args)[1];
  fprintf(stderr, "serverfd in forwarder: %d\n", serverfd);
  memset(buf1, 0, MAXLINE); // zero out buf1

  Pthread_detach(pthread_self());

  //rio_t server;
  //free(args);

  //Rio_readinitb(&client, clientfd);
  //Rio_readinitb(&server, serverfd);

  //while( (numBytes = Rio_readnb(&server, buf1, MAXLINE)) >= 0 ) {
  while ( (numBytes = Rio_readn(serverfd, buf1, MAXLINE)) >= 0) {
  //while ( (numBytes = Rio_readn(serverfd, buf1, MAXLINE)) >= 0) {
    //while( 1 ) {
      if ( numBytes > 0 ) {       
        Rio_writen(clientfd, buf1, MAXLINE);
        /*
        FILE *f = fopen("file.txt", "w");
        if (f == NULL)
        {
          printf("Error opening file!\n");
          exit(1);
        }

        fprintf(f, "Some text: %s\n", buf1);
        fclose(f);*/
        //memset(buf1, 0, sizeof(buf1)); // zero out buf1
      }
      else if (numBytes == 0){
        //Close(clientfd);
        //Close(serverfd);
        //shutdown(clientfd, 1);
        //return 0; - no, this is a bad change, makes pages load very slowly / not finish
      }
      else if (numBytes < 0){
        fprintf(stderr, "\nerror in forwarder, call shutdown(serverfd)? line is: %d\n", __LINE__);
        //shutdown(serverfd, 1);
      }

    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */
    
  }
  shutdown(clientfd,1);
  //shutdown(serverfd,1);
  return 0;

}


void ignore()
{
	signal(SIGPIPE, SIG_IGN);
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
