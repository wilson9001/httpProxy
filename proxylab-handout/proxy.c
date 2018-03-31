#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "csapp.h"
#include <string.h>
#include <time.h>
//#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
//#define BUFMAX 1000
#define BUFMAX 1049000
#define NTHREADS 4
#define SBUFSIZE 16
#define WIRELEN 2000

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

static const char *connectionHeader = "\r\nConnection: close";
static const char *proxyHeader = "\r\nProxy-Connection: close";
static const int wirelen = 2000;

static int FDbuffer[SBUFSIZE];
static char *logBuf[NTHREADS];
static int currentCacheSize;
static int readcnt;
sem_t mutex, w, timeMutex;

void *thread(void *vargp);
void *logThread();

/* $begin sbuft */
typedef struct
{
    int *buf;    /* Buffer array */
    int n;       /* Maximum number of slots */
    int front;   /* buf[(front+1)%n] is first item */
    int rear;    /* buf[rear%n] is last item */
    sem_t mutex; /* Protects accesses to buf */
    sem_t slots; /* Counts available slots */
    sem_t items; /* Counts available items */
} sbuf_t;
/* $end sbuft */

/* $begin logbuft */
typedef struct
{
    char **buf;  /* Buffer array */
    int n;       /* Maximum number of slots */
    int front;   /* buf[(front+1)%n] is first item */
    int rear;    /* buf[rear%n] is last item */
    sem_t mutex; /* Protects accesses to buf */
    sem_t slots; /* Counts available slots */
    sem_t items; /* Counts available items */
} logbuf_t;

typedef struct
{
    int objectSize;
    unsigned long timeLastAccessed;
    char request[WIRELEN];
    char data[MAX_OBJECT_SIZE];
} cachentry;

cachentry *cache[MAX_CACHE_SIZE];

//void sbuf_init(sbuf_t *sp, int n);
//void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

sbuf_t fileDescriptors;
logbuf_t logEntries;

cachentry entryFactory(int size, unsigned long time, char *req, char *dat)
{
    cachentry newEntry;
    newEntry.objectSize = size;
    newEntry.timeLastAccessed = time;
    strcpy(newEntry.request, req);
    strcpy(newEntry.data,dat);

    return newEntry;
}

int main(int argc, char **argv)
{
    printf("Begin main\n");
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    //initialize cache
    printf("Initializing cache\n");
    //memset(&cache, '\0', sizeof(cachentry) * MAX_CACHE_SIZE);
    for (int i = 0; i < MAX_CACHE_SIZE; i++)
    {
        cache[i] = NULL;
        /*cache[i].objectSize = 0;
        memset(&cache[i].request, '\0', wirelen);
        memset(&cache[i].data, '\0', MAX_OBJECT_SIZE);
        cache[i].timeLastAccessed = 0;*/
    }

    currentCacheSize = 0;
    readcnt = 0;
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    Sem_init(&timeMutex, 0, 1);

    printf("Creating file descriptor buffer\n");
    //create file descriptor buffer
    fileDescriptors.buf = FDbuffer;
    fileDescriptors.n = SBUFSIZE;                     /* Buffer holds max of n items */
    fileDescriptors.front = fileDescriptors.rear = 0; /* Empty buffer iff front == rear */
    Sem_init(&fileDescriptors.mutex, 0, 1);           /* Binary semaphore for locking */
    Sem_init(&fileDescriptors.slots, 0, SBUFSIZE);    /* Initially, buf has n empty slots */
    Sem_init(&fileDescriptors.items, 0, 0);           /* Initially, buf has zero data items */

    //create log buffer
    logEntries.buf = logBuf;
    logEntries.n = NTHREADS;
    logEntries.front = logEntries.rear = 0;
    Sem_init(&logEntries.mutex, 0, 1);
    Sem_init(&logEntries.slots, 0, SBUFSIZE);
    Sem_init(&logEntries.items, 0, 0);

    listenfd = Open_listenfd(argv[1]);

    for (int i = 0; i < NTHREADS; i++)
    {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    Pthread_create(&tid, NULL, logThread, NULL);

    while (true)
    {
        clientlen = sizeof(struct sockaddr_storage);
        //connfdp = Malloc(sizeof(int));
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        sbuf_insert(&fileDescriptors, connfd);
        //Pthread_create(&tid, NULL, thread, connfdp);
    }
}

void *logThread()
{
    Pthread_detach(pthread_self());

    FILE *log = fopen("proxyLog.txt", "a");

    if (!log)
    {
        fprintf(stderr, "Log file failed to initialize!\n");
        exit(EXIT_FAILURE);
    }

    printf("About to log opening string\n");

    fprintf(log, "---Begin Proxy Session---\n");
    fclose(log);

    while (true)
    {
        char *logEntry;
        P(&logEntries.items); /* Wait for available item */
        //P(&sp->mutex);                                                    /* Lock the buffer */
        logEntry = logEntries.buf[(++logEntries.front) % (logEntries.n)]; /* Remove the item */
        //V(&sp->mutex);                                                    /* Unlock the buffer */
        V(&logEntries.slots); /* Announce available slot */
        //return item;

        log = fopen("proxyLog.txt", "a");

        if (!log)
        {
            fprintf(stderr, "Log file failed to open!\n");
            exit(EXIT_FAILURE);
        }

        printf("%s\n", logEntry);

        fprintf(log, "%s", logEntry);

        fclose(log);
        log = NULL;
    }

    return NULL;
}

/* Thread routine */
void *thread(void *vargp)
{
    //int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    //Free(vargp);
    while (true)
    {
        int connfd = sbuf_remove(&fileDescriptors);

        printf("Thread created for connection on file descriptor %d.\n", connfd); // <--print functions are not thread safe... will need to remove.

        //Read data from client
        size_t n;
        char buf[BUFMAX];
        rio_t rioClient;

        Rio_readinitb(&rioClient, connfd);

        memset(buf, '\0', BUFMAX);
        n = Rio_readlineb(&rioClient, buf, BUFMAX);
        if (n < 0)
        {
            Close(connfd);

            continue;
            //return NULL;
        }

        //place request string in log buffer
        P(&logEntries.slots);                                       /* Wait for available slot */
        P(&logEntries.mutex);                                       /* Lock the buffer */
        logEntries.buf[(++logEntries.rear) % (logEntries.n)] = buf; /* Insert the item */
        V(&logEntries.mutex);                                       /* Unlock the buffer */
        V(&logEntries.items);                                       /* Announce available item */

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
            continue;

            //return NULL;
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

            continue;
            //return NULL;
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

            memcpy(portNumber, &buf[portSpot + 1], portLength);
            portNumber[portLength] = '\0';

            printf("Port specified: '%s'\n", portNumber);

            baseURLlength -= (portLength + 1);
        }

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

        URI[URIlength - 1] = '0';
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

        char buf2[BUFMAX];
        bool inCache = false;

        //check if cached. If so return cached object & update timestamp
       /* for (int i = 0; i < MAX_CACHE_SIZE; i++)
        {
            //lock writing mutex here?
            if (!strcmp(cache[i]->request, reqWire))
            {
                inCache = true;
                strcpy(buf2, cache[i]->request);
                n = cache[i]->objectSize;
                cache[i]->timeLastAccessed = (unsigned long)time(NULL);
                //unlock mutex here?
                break;
            }
            //unlock mutex here?
        }*/

        P(&mutex);
        readcnt++;
        if (readcnt == 1)
        {
            P(&w);
        }
        V(&mutex);

        for (int i = 0; i < MAX_CACHE_SIZE; i++)
        {
            if(cache[i])
            {
            if (!strcmp(cache[i]->request, reqWire))
            {
                inCache = true;
                printf("Item found in cache\n");
                strcpy(buf2, cache[i]->data);//This will need to be fixed to run to client from here.
                n = cache[i]->objectSize;
                P(&timeMutex);
                cache[i]->timeLastAccessed = (unsigned long)time(NULL);
                V(&timeMutex);
                break;
            }
        }
        }

        P(&mutex);
        readcnt--;
        if (!readcnt)
        {
            V(&w);
        }
        V(&mutex);

        if (!inCache)
        {
            //Connect to server
            int serverfd = open_clientfd(hostname, portNumber);

            if (serverfd < 0)
            {
                char *responseMessage = "HTTP/1.0 404 Not Found\r\n\r\n";
                printf("404\n");

                Rio_writen(connfd, responseMessage, strlen(responseMessage));

                Close(connfd);

                continue;
                //return NULL;
            }

            //Send request to server
            if (!Rio_writen(serverfd, reqWire, strlen(reqWire)))
            {
                char *responseMessage = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
                printf("500\n");
                Rio_writen(connfd, responseMessage, strlen(responseMessage));
                Close(serverfd);
                Close(connfd);
                continue;
                //return NULL;
            }

            //Read response and copy to client

            //char buf2[BUFMAX];
            rio_t rioServer;

            unsigned int offset = 0;
            char cacheData[MAX_OBJECT_SIZE];
            memset(cacheData, '\0', MAX_OBJECT_SIZE);

            bool tooBig = false;

            Rio_readinitb(&rioServer, serverfd);
            while ((n = Rio_readlineb(&rioServer, buf2, BUFMAX)) != 0)
            {
                if (n < 0)
                {
                    Close(serverfd);
                    char *errorMsg = "\n\n\nHTTP/1.0 500 Internal Server Error\r\n\r\n";
                    Rio_writen(connfd, errorMsg, strlen(errorMsg));
                    Close(connfd);
                    continue;
                    //return NULL;
                }

                if (!tooBig && ((offset + n) < MAX_OBJECT_SIZE))
                {
                    strcat(&cacheData[offset], buf2);
                    //strcpy(&cacheData[offset], buf2);
                    offset += n;
                }
                else
                {
                    tooBig = true;
                }

                Rio_writen(connfd, buf2, n);
            }

            printf("200?\n");

            Close(serverfd);
            Close(connfd);

           if (!tooBig)
            {
                //log item to cache
                printf("About to log item to cache\n");
                P(&w);
                if ((currentCacheSize + strlen(cacheData)) < MAX_CACHE_SIZE)
                {
                    //create new entry
                    for (int i = 0; i < MAX_CACHE_SIZE; i++)
                    {
                        if (!cache[i])
                        {
                            cache[i] = malloc(sizeof(cachentry));
                            //cache[i] = cachentry;

                            strcpy(cache[i]->data, cacheData);
                            cache[i]->objectSize = strlen(cacheData);
                            strcpy(cache[i]->request, reqWire);
                            cache[i]->timeLastAccessed = (unsigned long)time(NULL);
                            currentCacheSize += cache[i]->objectSize;
                            break;
                        }
                    }
                }
                else
                {
                    //Remove oldest entry from cache and then keep removing until there is enough space
                    int lastCache = 0;

                    while (!cache[lastCache])
                    {
                        lastCache++;
                    }

                    unsigned long oldestDateTime = cache[lastCache]->timeLastAccessed;

                    for (int i = 0; i < MAX_CACHE_SIZE; i++)
                    {
                        if(cache[i])
                        {
                            if(cache[i]->timeLastAccessed < oldestDateTime)
                            {
                                lastCache= i;
                                oldestDateTime = cache[i]->timeLastAccessed;
                            }
                        }
                        /*if (!cache[i].objectSize)
                        {
                            continue;
                        }
                        if (cache[i].timeLastAccessed < oldestDateTime)
                        {
                            lastCache = i;
                            oldestDateTime = cache[i].timeLastAccessed;
                        }*/
                    }

                    memset(&cache[lastCache]->data, '\0', MAX_OBJECT_SIZE);
                    memset(&cache[lastCache]->request, '\0', wirelen);
                    currentCacheSize -= cache[lastCache]->objectSize;

                    strcpy(cache[lastCache]->data, cacheData);
                    cache[lastCache]->objectSize = strlen(cacheData);
                    strcpy(cache[lastCache]->request, reqWire);
                    cache[lastCache]->timeLastAccessed = (unsigned long)time(NULL);
                    currentCacheSize += cache[lastCache]->objectSize;

                    if (currentCacheSize > MAX_CACHE_SIZE)
                    {
                        while (currentCacheSize > MAX_CACHE_SIZE)
                        {
                            lastCache = 0;

                            while (!cache[lastCache])
                            {
                                lastCache++;
                            }

                            oldestDateTime = cache[lastCache]->timeLastAccessed;

                            for (int i = 0; i < MAX_CACHE_SIZE; i++)
                            {
                                if(cache[i])
                                {
                                    if(cache[i]->timeLastAccessed < oldestDateTime)
                                    {
                                        lastCache = i;
                                        oldestDateTime = cache[i]->timeLastAccessed;
                                    }
                                }
                                /*if (!cache[i].objectSize)
                                {
                                    continue;
                                }
                                if (cache[i].timeLastAccessed < oldestDateTime)
                                {
                                    lastCache = i;
                                    oldestDateTime = cache[i].timeLastAccessed;
                                }*/
                            }

                            /*memset(&cache[lastCache].data, '\0', MAX_OBJECT_SIZE);
                            memset(&cache[lastCache].request, '\0', wirelen);
                            cache[lastCache].timeLastAccessed = 0;*/
                            currentCacheSize -= cache[lastCache]->objectSize;
                            //cache[lastCache].objectSize = 0;
                            free(&cache[lastCache]);
                        }
                    }
                }
                V(&w);
            }
        }
        else
        {
            Rio_writen(connfd, buf2, n);
            close(connfd);
        }
    }

    //Have to figure out how to make code go here...
    //sbuf_deinit(&fileDescriptors);
    return NULL;
}

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                           /* Wait for available item */
    P(&sp->mutex);                           /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
    V(&sp->mutex);                           /* Unlock the buffer */
    V(&sp->slots);                           /* Announce available slot */
    return item;
}
/* $end sbuf_remove */
/* $end sbufc */