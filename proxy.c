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
  int clientfd2;
  
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
    //clientfd2 = connfd;

    /* you have to write the code to process this new client request */

    /* create a new thread (or two) to process the new connection */

    //int newargv[] = {connfd, serverPort, clientfd2};
  if (connfd >= 0){
    //int newargv[] = {connfd, serverPort};
    int *newargv = malloc(2 * sizeof(newargv));
    newargv[0] = connfd;
    newargv[1] = serverPort;
    Pthread_create(&tid, NULL, webTalk, newargv);
    //webTalk(newargv);
  }
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

void *webTalk(void* args)
{
  int numBytes, lineNum, serverfd, clientfd, serverPort, clientfd2;

  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  //clientfd2 = ((int*)args)[2];

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
  ssize_t rio_return = Rio_readlineb(&client, buf1, MAXLINE);

  // Determine protocol (CONNECT or GET)
  fprintf(stderr, "\nBROWSER'S INITIAL REQ: %s\n", buf1);
  
  if (buf1[0] == 'G'){

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
    
    parseAddress(uri, host, file_ptr, serverPort_ptr);


// GET: open connection to webserver (try several times, if necessary)

    serverfd = Open_clientfd(host, serverPort);
    
    int break_me = 0;
/*
    while (break_me == 0){
      Rio_readlineb(&client, buf3, MAXLINE);
      if (buf3[0] == '\r' && buf3[1] == '\n'){
        ++break_me;
      } 
      strcat(buf2, buf3);
    }

    //fprintf(stderr, "buf2 before suppression: \n%s", buf2);

    int size_to_erase = strlen("keep-alive");
    char closeReplace[size_to_erase];
    strcpy(closeReplace, "close     \0");


    while ( strstr(buf2, "keep-alive") != NULL ){
      strncpy( (strstr(buf2, "keep-alive")), closeReplace, strlen(closeReplace) );
    }
  */ 

  // JUST DROP KEEP-ALIVE, DON'T REPLACE IT WITH CLOSE

    while (break_me == 0){
      Rio_readlineb(&client, buf3, MAXLINE);
      if ( !strcasecmp(buf3, "Connection: Keep-Alive\r\n") || !strcasecmp(buf3, "Proxy-Connection: Keep-Alive\r\n") ){
        continue;
      }
      if (buf3[0] == '\r' && buf3[1] == '\n'){
        ++break_me;
      } 
      strcat(buf2, buf3);
    }

    fprintf(stderr, "buf2 after suppression: \n%s", buf2); 

    //write to serverfd to send data to server
    int n;
    n = Rio_writen(serverfd, buf2, MAXLINE);
    if (n < 0){
      fprintf(stderr, "error sending get request to server\n");
    }
    //memset(buf3, 0, MAXLINE); //zero out buf3 so we can receive data on it

    // GET: Transfer remainder of the request

    // GET: now receive the response
    /*
    int err = 0;
    int thing = 0;
    while (err < 10){
      thing = Rio_readn(serverfd, buf1, MAXLINE);
      if (thing < 0){
        ++err;
        fprintf(stderr, "err: %d\n", err);
        if ((serverfd = Open_clientfd(host, serverPort)) < 0){
          fprintf(stderr, "error line: %d\n", __LINE__);
          }
        n = Rio_writen(serverfd, buf2, MAXLINE);
        if (n < 0){
          fprintf(stderr, "error sending get request to server line: %d\n", __LINE__);
        }
      }
  }*/

      int *args2 = malloc(2 * sizeof(args2));
      args2[0] = clientfd;
      args2[1] = serverfd;
    //args2[2] = clientfd2;
    //int args2[] = {clientfd, serverfd};
      pthread_t tid2;
      Pthread_create(&tid2, NULL, &forwarder, args2);
    //forwarder(args2);

    }

    else if (buf1[0] == 'C'){
      fprintf(stderr, "\nWe should process a CONNECT request\n");

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

      fprintf(stderr, "Calling secureTalk at line: %d\n", __LINE__);

      secureTalk(clientfd, client, host, version, serverPort);
  
  }
  
//Close(serverfd);
//Close(clientfd);
pthread_exit(NULL);

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

  //    OPEN TCP CONNECTION ON PORT 443 

      if ( (serverfd = Open_clientfd(host, serverPort)) > 0 ) {
        fprintf(stderr, "TCP success in CONNECT logic line: %d\n", __LINE__);
      }

//    SEND CONNECT REQUEST TO SERVER
      /*
      int n;
      strcat(buf1, "\r\n\r\n");
      //char tosend[MAXLINE]; 
      //strcpy(tosend, "CONNECT yahoo.com:443\r\n");
      n = Rio_writen(serverfd, buf1, MAXLINE);

      if (n <= 0){
        fprintf(stderr, "error sending CONNECT request to server: line %d\n", __LINE__);
      }
      fprintf(stderr, "sent server buf1: \n%s", buf1);
    */

//    SEND HTTP 200 OK TO CLIENT

      char SSL_msg[MAXLINE];
      strcpy(SSL_msg, "HTTP/1.1 200 OK\r\n\r\n");
      fprintf(stderr, "SSL_msg is: %s\n", SSL_msg);
      int checker = Rio_writen(clientfd, SSL_msg, strlen(SSL_msg));
      if (checker == strlen(SSL_msg)){
        fprintf(stderr, "successfully wrote HTTP 200 to client. ");
      }
      else if (checker < 0){
        fprintf(stderr, "error writing HTTP 200 to client. line: %d\n", __LINE__);
      }

//    CALL FORWARDER_SSL

      int *args3 = malloc(2 * sizeof(args3));
      args3[0] = clientfd;
      args3[1] = serverfd;
      //args3[2] = clientfd2;
      pthread_t tid3;
      shutdown(serverfd, 1);
      Pthread_create(&tid3, NULL, &forwarder_SSL, args3);

//    SEND BYTES FROM CLIENT TO SERVER

    pthread_mutex_lock(&mutex);
      //int numBytes2;
      while ( (numBytes2 = Rio_readp(clientfd, buf1, MAXLINE)) > 0) {
      if ( numBytes2 > 0 ) {       
        Rio_writen(serverfd, buf1, numBytes2);
      }
    }
    if (numBytes2 < 0){
      fprintf(stderr, "error line: %d\n", __LINE__);
    }
    shutdown(serverfd, 1);

    pthread_mutex_unlock(&mutex);

    //Close(clientfd);
    //Close(serverfd);


}

/* this function is for passing bytes from origin server to client */

void *forwarder_SSL(void* args){
  int numBytes, lineNum, serverfd, clientfd;
  //int clientfd2;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0]; 
  serverfd = ((int*)args)[1];
  //clientfd2 = ((int*)args)[2];

  Pthread_detach(pthread_self());
  /*
  while ( (Rio_readp(serverfd, buf1, MAXLINE)) >= 0 ){
    Rio_writep(clientfd, buf1, MAXLINE);
  }*/


//    SEND BYTES FROM CLIENT TO SERVER
/*
      while ( (numBytes = Rio_readp(clientfd, buf1, MAXLINE)) > 0) {
      if ( numBytes > 0 ) {       
        Rio_writen(serverfd, buf1, MAXLINE);
      }
    }
    if (numBytes < 0){
      fprintf(stderr, "error line: %d\n", __LINE__);
    }
*/
    int cntr1 = 0;
    fprintf(stderr, "\nat line: %d\n", __LINE__);
    numBytes = Rio_readp(serverfd, buf1, MAXLINE);
    fprintf(stderr, "numBytes: %d\n", numBytes);
    if (numBytes < 0){
      fprintf(stderr, "error at line: %d\n", __LINE__);
    }

    while ( numBytes > 0 ){
      Rio_writen(clientfd, buf1, numBytes);
      ++cntr1;
      //fprintf(stderr, "Wrote the following to client fd: %s\n", buf1);
      numBytes = Rio_readp(serverfd, buf1, MAXLINE);
    }

    fprintf(stderr, "at line: %d\n", __LINE__);
    shutdown(clientfd, 1);

//    SEND BYTES FROM CLIENT TO SERVER
/*
      while ( (numBytes = Rio_readp(clientfd, buf1, MAXLINE)) > 0) {
      if ( numBytes > 0 ) {       
        Rio_writen(serverfd, buf1, MAXLINE);
      }
    }
    if (numBytes < 0){
      fprintf(stderr, "error line: %d\n", __LINE__);
    }
*/
// now pass bytes from client to server 
    /*
    int ckr;
    int cntr2 = 0;

    ckr = Rio_readp(clientfd, buf1, MAXLINE);
    fprintf(stderr, "value of Rio_readp(clientfd) (SSL) is: %d at line: %d\n", ckr, __LINE__);
    fprintf(stderr, "client wants to send this message: %sENDMSG\n", buf1);

    while ( ckr >= 0 && cntr2 < 10 ){    
      ++cntr2;
      int ckr2 = Rio_writen(serverfd, buf1, MAXLINE);
      if (ckr2 < 0){
          //Close(serverfd);
        break;
      }
      fprintf(stderr, "wrote %d many bytes to serverfd (SSL) at line: %d\n", ckr2, __LINE__);
    }
    
    if (ckr < 0){
      fprintf(stderr, "error in passing bytes from Client to Server (SSL). LINE: %d\n", __LINE__);
    }

    fprintf(stderr, "at line: %d\n", __LINE__);
    
    /*
    else if (numBytes == 0){
      //shutdown(clientfd, 1);
    }
    else if (numBytes < 0){
      fprintf(stderr, "error in forwarder_SSL, LINE: %d\ncall shutdown(serverfd)?", __LINE__);
    }
  */

/*
    if (clientfd){
      Close(clientfd);
    }
    if (serverfd) {
      Close(serverfd);
    }*/
    pthread_exit(NULL);

  }

  void *forwarder(void* args)
  {
    int numBytes, lineNum, serverfd, clientfd;
  //int clientfd2;
    int byteCount = 0;
    char buf1[MAXLINE];
    clientfd = ((int*)args)[0];
    serverfd = ((int*)args)[1];
  //clientfd2 = ((int*)args)[2];
  //memset(buf1, 0, MAXLINE); // zero out buf1

    Pthread_detach(pthread_self());
    //pthread_mutex_lock(&mutex);
    while ( (numBytes = Rio_readn(serverfd, buf1, MAXLINE)) > 0) {
      if ( numBytes > 0 ) {       
        Rio_writen(clientfd, buf1, MAXLINE);
      }
    }
    if (numBytes <= 0){
      fprintf(stderr, "error line: %d\n", __LINE__);
      Close(serverfd);
    }
    //pthread_mutex_unlock(&mutex);
      /*
      else if (numBytes == 0){
        //Close(clientfd);
        //Close(serverfd);
        //shutdown(clientfd, 1);
        //return 0; - no, this is a bad change, makes pages load very slowly / not finish
      }
      else if (numBytes < 0){
        fprintf(stderr, "\nerror in forwarder, call shutdown(serverfd)? line is: %d\n", __LINE__);
        //shutdown(serverfd, 1);
      }*/

    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */

  //shutdown(clientfd,1);
  //shutdown(serverfd,1);
        Close(clientfd);
        Close(serverfd);
        pthread_exit(NULL);

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
