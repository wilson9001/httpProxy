# httpProxy
A simple threaded http proxy server.

##Part I: Implementing a sequential web proxy
---------------------------------------------------------------------------------
The first step is implementing a basic sequential proxy that handles HTTP/1.0 GET requests. Other request types, such as POST, are strictly optional. When started, your proxy should listen for incoming connections on a port whose number will be specified on the command line. Once a connection is established, your proxy should read the entirety of the request from the client and parse the request. It should determine whether the client has sent a valid HTTP request. If so, it can then establish its own connection to the appropriate web server then request the object the client specified. Finally, your proxy should read the server’s response and forward it to the client.

###HTTP/1.0 GET requests
When an end user enters a URL such as http://www.cmu.edu/hub/index.html into the address bar of a web browser, the browser will send an HTTP request to the proxy that begins with a line that might resemble the following:

`GET http://www.cmu.edu/hub/index.html HTTP/1.1`

In that case, the proxy should parse the request into at least the following fields: the hostname, www.cmu.edu, and the path or query and everything following it, /hub/index.html. That way, the proxy can determine that it should open a connection to www.cmu.edu and send an HTTP request of its own starting with a line of the following form:

`GET /hub/index.html HTTP/1.0`

Note that all lines in an HTTP request end with a carriage return, `\r`, followed by a newline, `\n`. Also important is that every HTTP request is terminated by an empty line: `\r\n`. You should notice in the above example that the web browser’s request line ends with `HTTP/1.1`, while the proxy’s request line ends with `HTTP/1.0`. Modern web browsers will generate HTTP/1.1 requests, but your proxy should handle them and forward them as HTTP/1.0 requests.

It is important to consider that HTTP requests, even just the subset of HTTP/1.0 GET requests, can be incredibly complicated. The textbook describes certain details of HTTP transactions, but you should refer to RFC 1945 for the complete HTTP/1.0 specification. Ideally your HTTP request parser will be fully robust according to the relevant sections of RFC 1945, except for one detail: while the specification allows for multiline request fields, your proxy is not required to properly handle them. Of course, your proxy should never prematurely abort due to a malformed request.

###Request headers
The important request headers for this lab are the Host, User-Agent, Connection, and Proxy-Connection headers:

* Always send a Host header. While this behavior is technically not sanctioned by the HTTP/1.0 specification, it is necessary to coax sensible responses out of certain Web servers, especially those that use virtual hosting. The Host header describes the hostname of the end server. For example, to access http://www.cmu.edu/hub/index.html, your proxy would send the following header:`Host: www.cmu.edu`. It is possible that web browsers will attach their own Host headers to their HTTP requests. If that is the case, your proxy should use the same Host header as the browser.
* You may choose to always send the following User-Agent header: `User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3` The header is provided on two separate lines because it does not fit as a single line in the writeup, but your proxy should send the header as a single line. The User-Agent header identifies the client (in terms of parameters such as the operating system and browser), and web servers often use the identifying information to manipulate the content they serve. Sending this particular User-Agent: string may improve, in content and diversity, the material that you get back during simple telnet-style testing.
* Always send the following Connection header: `Connection: close`
* Always send the following Proxy-Connection header: `Proxy-Connection: close` The Connection and Proxy-Connection headers are used to specify whether a connection will be kept alive after the first request/response exchange is completed. It is perfectly acceptable (and suggested) to have your proxy open a new connection for each request. Specifying close as the value of these headers alerts web servers that your proxy intends to close connections after the first request/response exchange. For your convenience, the values of the described User-Agent header is provided to you as a string constant in proxy.c.
* Finally, if a browser sends any additional request headers as part of an HTTP request, your proxy should forward them unchanged.

###Port numbers
There are two significant classes of port numbers for this lab: HTTP request ports and your proxy’s listening port. The HTTP request port is an optional field in the URL of an HTTP request. That is, the URL may be of the form, http://www.cmu.edu:8080/hub/index.html, in which case your proxy should connect to the host www.cmu.edu on port 8080 instead of the default HTTP port, which is port 80. Your proxy must properly function whether or not the port number is included in the URL. The listening port is the port on which your proxy should listen for incoming connections. Your proxy should accept a command line argument specifying the listening port number for your proxy. For example, with the following command, your proxy should listen for connections on port 15213: `linux> ./proxy 15213`

You may select any non-privileged listening port (greater than 1,024 and less than 65,536) as long as it is not used by other processes. Since each proxy must use a unique listening port and many people will simultaneously be working on each machine, the script port-for-user.pl is provided to help you pick your own personal port number. Use it to generate port number based on your user ID:
`linux> ./port-for-user.pl droh`
`droh: 45806`

The port, p, returned by port-for-user.pl is always an even number. So if you need an additional port number, say for the Tiny server, you can safely use ports p and p + 1. Please don’t pick your own random port. If you do, you run the risk of interfering with another user.

##Part II: Dealing with multiple concurrent requests
---------------------------------------------------------------------------------
Once you have a working sequential proxy, you should alter it to simultaneously handle multiple requests. The simplest way to implement a concurrent server is to spawn a new thread to handle each new connection request.
* Note that your threads should run in detached mode to avoid memory leaks.
* The open clientfd and open listenfd functions described in the CS:APP3e textbook are based on the modern and protocol-independent getaddrinfo function, and thus are thread safe.