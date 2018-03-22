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

static const char *connectionHeader = "\r\nConnection: close";
static const char *proxyHeader = "\r\nProxy-Connection: close";
static const int wirelen = 2000;

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
    printf("Thread created for connection on file descriptor %d.\n", connfd); // <--print functions are not thread safe... will need to remove.

    //Read data from client
    size_t n;
    char buf[BUFMAX];
    rio_t rioClient;

    Rio_readinitb(&rioClient, connfd);

    //while(true)
    //{
    memset(buf, '\0', BUFMAX);
    n = Rio_readlineb(&rioClient, buf, BUFMAX);
    if(n < 0)
    {
       Close(connfd);
        //continue;
        return NULL;
    }

    //parse request to get address, port, URI, and headers.
    char *hostname;
    char portNumber[8];
    memset(portNumber, '\0', 8);

    char reqWire[wirelen * 2];

    memset(reqWire, '\0', wirelen);

    char headerTestSubstr[6];
    memcpy(headerTestSubstr, buf, 3);
    headerTestSubstr[3] = '\0';

    printf("Method: '%s'\n", headerTestSubstr);

    if (strcmp(headerTestSubstr, "GET") && strcmp(headerTestSubstr, "get") && strcmp(headerTestSubstr, "Get"))
    {
        //generate bad method response to client
        char *responseMessage = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        printf("405\n");
        Rio_writen(connfd, responseMessage, strlen(responseMessage));

        Close(connfd);

        return NULL;
        //continue;
    }

    unsigned int reqBufPos = 4;

    memcpy(headerTestSubstr, &buf[reqBufPos], 5);
    headerTestSubstr[5] = '\0';

    printf("HTTP protocol: '%s'\n", headerTestSubstr);

    if (strcmp(headerTestSubstr, "http:") && strcmp(headerTestSubstr, "https"))
    {
        //generate malformed request response to client
        char *responseMessage = "HTTP/1.0 400 Bad Request\r\n\r\n";
        printf("400\n");

        Rio_writen(connfd, responseMessage, strlen(responseMessage));

        Close(connfd);

        return NULL;
        //continue;
    }

    reqBufPos += 7;

    if (!strcmp(headerTestSubstr, "https"))
    {
        printf("https detected. Switching to http\n");

        reqBufPos++;
    }

    unsigned int baseURLlength = 0;
    unsigned int portSpot = 0;
    unsigned int baseURLWithPortLength = 0;

    while (buf[reqBufPos + baseURLlength] != '/' && buf[reqBufPos + baseURLlength] != ' ')
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
        portLength = (reqBufPos + baseURLlength) - (portSpot + 1);

        //char portN[portLength];
        memcpy(portNumber, &buf[portSpot + 1], portLength);
        portNumber[portLength] = '\0';

        //portNumber = portN;
        printf("Port specified: '%s'\n", portNumber);

        baseURLlength -= (portLength + 1);
    }

    //printf("Value outside if: '%s'\n", portNumber);

    char baseURL[baseURLlength + 1];

    memcpy(baseURL, &buf[reqBufPos], baseURLlength);
    baseURL[baseURLlength] = '\0';

    printf("Base URL: '%s'\n", baseURL);

    //begin URI, skipping first / in case the user doesn't put it
    if (buf[reqBufPos + baseURLWithPortLength] == '/')
    {
        reqBufPos += baseURLWithPortLength + 1;
    }
    else
    {
        reqBufPos += baseURLWithPortLength;
    }
    size_t URIlength = 0;



    while (buf[reqBufPos + URIlength] != '\r' && buf[reqBufPos + URIlength + 1] != '\n')
    {
        URIlength++;
    }

    URIlength++;

    char URI[URIlength + 1];

    memcpy(URI, &buf[reqBufPos], URIlength);

    URI[URIlength -1] = '0';
    URI[URIlength] = '\0';

    printf("URI: '%s'\n", URI);

    reqBufPos += URIlength + 2; //skip the \r\n, pointing after \n

    //copy headers
    printf("Initializing request to server...\n");

    char passThroughHeaders[wirelen];

    memset(passThroughHeaders, '\0', wirelen);
    printf("Request initialized.\n");
    strcpy(passThroughHeaders, &buf[reqBufPos]);
    printf("Pass through headers are:\n'%s'\n", passThroughHeaders);

    //determine if required headers are already included
    char *hostHeaderBegin = strstr(passThroughHeaders, "Host:");
    char *connectHeaderBegin = strstr(passThroughHeaders, "Connection:");
    char *proxyConnectHeaderBegin = strstr(passThroughHeaders, "Proxy-Connection:");
    char *userAgentHeaderBegin = strstr(passThroughHeaders, "User-Agent:");

    //begin assembling request string for server endpoint
    strcat(reqWire, "GET /");

    strcat(reqWire, URI);
    //printf("Teapot\n");

    strcat(reqWire, "\r\n");

    printf("Building request:\n'%s'\n", reqWire);

    if (!hostHeaderBegin)
    {
        char hostHeader[200];
        memset(hostHeader, '\0', 200);
        strcat(hostHeader, "Host: ");
        strcat(hostHeader, baseURL);
        strcat(reqWire, hostHeader);
        printf("'%s'\n", reqWire);
    }

    if (!connectHeaderBegin)
    {
        strcat(reqWire, connectionHeader);
        printf("'%s'\n", reqWire);
    }

    if (!proxyConnectHeaderBegin)
    {
        strcat(reqWire, proxyHeader);
        printf("'%s'\n", reqWire);
    }

    if (!userAgentHeaderBegin)
    {
        strcat(reqWire, user_agent_hdr);
        printf("'%s'\n", reqWire);
    }

    strcat(reqWire, passThroughHeaders);
    strcat(reqWire, "\r\n\r\n");

    printf("'%s'\n", reqWire);

    hostname = baseURL;

    if (!portSpot)
    {
        strcat(portNumber, "80");
    }

    printf("Port number just before openfd: %s\n", portNumber);

    //Connect to server
    int serverfd = open_clientfd(hostname, portNumber);

    if (serverfd < 0)
    {
        char *responseMessage = "HTTP/1.0 404 Not Found\r\n\r\n";
        printf("404\n");

        Rio_writen(connfd, responseMessage, strlen(responseMessage));

        Close(connfd);

        return NULL;
        //continue;
    }

    //printf("%s", reqWire);

    //Send request to server
    if(!Rio_writen(serverfd, reqWire, strlen(reqWire)))
    {
        char *responseMessage = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        printf("500\n");
        Rio_writen(connfd, responseMessage, strlen(responseMessage));
        //printf("About to close serverfd %d.\n", serverfd);
        Close(serverfd);
        //printf("About to close connfd %d.\n", connfd);
        Close(connfd);
        return NULL;
        //continue;
    }

    //Read response and copy to client

    char buf2[BUFMAX];
    rio_t rioServer;

    Rio_readinitb(&rioServer, serverfd);
    while ((n = Rio_readlineb(&rioServer, buf2, BUFMAX)) != 0)
    {
        if(n < 0)
        {
            Close(serverfd);
            char *errorMsg = "\n\n\nHTTP/1.0 500 Internal Server Error\r\n\r\n";
            Rio_writen(connfd, errorMsg, strlen(errorMsg));
            //break;
            Close(connfd);
            return NULL;
        }

        Rio_writen(connfd, buf2, n);
    }
    printf("200?\n");

    Close(serverfd);
    //}
    Close(connfd);
    return NULL;
}