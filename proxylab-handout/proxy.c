#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "csapp.h"
#include <string.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define BUFMAX 1000
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

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

    //parse request to get address, port, URI, and headers.
    char *hostname;
    char *portNumber = NULL;
    char *reqWire;

    char headerTestSubstr[6];
    memcpy(headerTestSubstr, buf, 3);
    headerTestSubstr[3] = '\0';

    strupr(headerTestSubstr);

    if (!strcmp(headerTestSubstr, "GET"))
    {
        //generate bad method response to client
        char *responseMessage = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";

        Rio_writen(connfd, responseMessage, sizeof(responseMessage));

        Close(connfd);

        return NULL;
    }

    unsigned int reqBufPos = 4;

    memcpy(headerTestSubstr, &buf[reqBufPos], 5);
    headerTestSubstr[5] = '\0';

    if (strcmp(headerTestSubstr, "http:") || strcmp(headerTestSubstr, "https"))
    {
        //generate malformed request response to client
        char *responseMessage = "HTTP/1.0 400 Bad Request\r\n\r\n";

        Rio_writen(connfd, responseMessage, sizeof(responseMessage));

        Close(connfd);

        return NULL;
    }

    reqBufPos += 7;

    if (strcmp(headerTestSubstr, "https"))
    {
        reqBufPos++;
    }

    unsigned int baseURLlength = 0;
    unsigned int portSpot = 0;
    unsigned int baseURLWithPortLength = 0;

    while (buf[reqBufPos + baseURLlength] != '/')
    {
        if (buf[reqBufPos + baseURLlength] == ':')
        {
            portSpot = reqBufPos + baseURLlength;
        }

        baseURLlength++;
    }

    baseURLWithPortLength = baseURLlength;

    int portLength = 0;

    if (portSpot)
    {
        portLength = (reqBufPos + baseURLlength) - (portSpot /*+1*/);

        char portN[portLength /*+ 1*/];
        memcpy(portN, &buf[portSpot + 1], portLength);
        portN[portLength] = '\0';

        portNumber = portN;

        baseURLlength -= (portLength + 1);
    }

    char baseURL[baseURLlength + 1];

    memcpy(baseURL, &buf[reqBufPos], baseURLlength);
    baseURL[baseURLlength] = '\0';

    reqBufPos += baseURLWithPortLength + 1;

    size_t URIlength = 0;

    while (buf[reqBufPos + URIlength] != '\r' && buf[reqBufPos + URIlength + 1] != '\n')
    {
        URIlength++;
    }

    char URI[URIlength + 1];

    memcpy(URI, &buf[reqBufPos], URIlength);

    URI[URIlength - 1] = '0';
    URI[URIlength] = '\0';

    reqBufPos += URIlength;

    //copy headers
    char passThroughHeaders[2000];

    memset(passThroughHeaders, '\0', sizeof(passThroughHeaders));
    strcpy(passThroughHeaders, &buf[reqBufPos]);

    //determine if required headers are already included
    char *hostHeaderBegin = strstr(passThroughHeaders, "Host:");
    char *connectHeaderBegin = strstr(passThroughHeaders, "Connection:");
    char *proxyConnectHeaderBegin = strstr(passThroughHeaders, "Proxy-Connection:");
    char *userAgentHeaderBegin = strstr(passThroughHeaders, "User-Agent:");

    //begin assembling request string for server endpoint
    reqWire = "GET /";

    strcat(reqWire, URI);

    if (!hostHeaderBegin)
    {
        char *hostHeader = "Host: ";
        strcat(hostHeader, baseURL);
        strcat(reqWire, hostHeader);
    }

    if (!connectHeaderBegin)
    {
        strcat(reqWire, "\r\nConnection: close");
    }

    if (!proxyConnectHeaderBegin)
    {
        strcat(reqWire, "\r\nProxy-Connection: close");
    }

    if (!userAgentHeaderBegin)
    {
        strcat(reqWire, user_agent_hdr);
    }

    strcat(reqWire, passThroughHeaders);

    hostname = baseURL;

    if (!portNumber)
    {
        portNumber = "80";
    }

    //Connect to server
    int serverfd = open_clientfd(hostname, portNumber);

    if (serverfd == -1)
    {
        //fprintf(stderr, "Failed to connect to host %s:%s\n", hostname, port);
        //TODO: generate not found response for client

        char *responseMessage = "HTTP/1.0 404 Not Found\r\n\r\n";

        Rio_writen(connfd, responseMessage, sizeof(responseMessage));

        Close(connfd);

        return NULL;
    }

    //Send request to server
    Rio_writen(serverfd, reqWire, strlen(reqWire));

    //Read response and copy to client

    char buf2[BUFMAX];
    rio_t rioServer;

    Rio_readinitb(&rioServer, serverfd);
    while ((n = Rio_readlineb(&rioServer, buf2, BUFMAX)) != 0)
    {
        Rio_writen(connfd, buf2, n);
    }

    Close(serverfd);
    Close(connfd);
    return NULL;
}