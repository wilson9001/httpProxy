#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define BUFMAX 1000
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

static const char *connectionHeader = "Connection: close";
static const char *proxyHeader = "Proxy-Connection: close";

/*typedef struct request request, *request;
struct request
{

};*/

typedef struct request
{
    unsigned char *providedHeaders;
    //request *nextHeader;

} request;

void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* Thread routine */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    //printf("Thread created for connection on file descriptor %d.\n", connfd); <--print functions are not thread safe...

    //Read data from client
    size_t n;
    char buf[BUFMAX];
    rio_t rioClient;

    Rio_readinitb(&rioClient, connfd);
    n = Rio_readlineb(&rioClient, buf, BUFMAX);

    //TODO: parse request to get address, port, URI, and headers.
    char *hostname;
    char *port;
    char *reqWire;

    unsigned int reqBufPos = 0;




    //Connect to server
    int serverfd = open_clientfd(hostname, port);

    if(serverfd == -1)
    {
        //fprintf(stderr, "Failed to connect to host %s:%s\n", hostname, port);
        //TODO: generate error response for client



        return NULL;
    }

    //Send request to server
    Rio_writen(serverfd, reqWire, strlen(reqWire));

    //Read response and copy to client
    
    char buf2[BUFMAX];
    rio_t rioServer;

    Rio_readinitb(&rioServer, serverfd);
    while((n = Rio_readlineb(&rioServer, buf2, BUFMAX)) != 0)
    {
        Rio_writen(connfd, buf2, n);
    }

    Close(serverfd);
    Close(connfd);
    return NULL;
}