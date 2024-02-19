#include <iostream>
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Format.h"
#include <poll.h>
#include <stdlib.h>
#include <string>

using namespace Poco::Net;
using namespace std;

void exit_usage()
{
    cerr
        << "Usage: wscatd <websocket_port> [<ip_addr>] <tcp_port>\n"
        << " <websocket_port> - incoming port\n"
        << " <ip_addr> - destination address [127.0.0.1]\n"
        << " <tcp_port> - tcp port\n";
    exit(2);
}

class WebHandler: public HTTPRequestHandler
{
public:
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
    {
        response.setChunkedTransferEncoding(true);
        response.setContentType("text/plain");
        std::ostream& ostr = response.send();
        ostr << "OK" << endl;
    }
};

class WebsockHandler: public HTTPRequestHandler
{
public:
    static SocketAddress tcpsock_addr;
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
    {
        WebSocket ws(request, response);
        StreamSocket ts(tcpsock_addr);
        const int sz=65536;
        char data[sz+4096],*buf=(char *)((1+(size_t)data/4096)*4096);
        int flags;
        int n;
        struct pollfd fds[2] = {
            {ts.impl()->sockfd(),POLLIN,0},
            {ws.impl()->sockfd(),POLLIN,0}
        };       
        for(;;) {
            while ((n=poll(fds,2,-1))<0 && errno == EINTR);
                
            if (n<0) break;
                
            if (fds[0].revents&POLLIN) {
                n = ts.receiveBytes(buf,sz);
                if (n>0)
                    ws.sendFrame(buf,n,WebSocket::FRAME_BINARY);
                else
                    break;
            }

            if (fds[1].revents&POLLIN) {                    
                n = ws.receiveFrame(buf,sz,flags);
                if (n>0) 
                    ts.sendBytes(buf,n);
                else if (flags&WebSocket::FRAME_OP_PING)
                    ws.sendFrame(buf,n,WebSocket::FRAME_OP_PONG|WebSocket::FRAME_FLAG_FIN);
                else
                    break;
            }
        }
        ws.close(),ts.close();        
    }
};

SocketAddress WebsockHandler::tcpsock_addr;

class RequestHandlerFactory: public HTTPRequestHandlerFactory
{
public:
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request)
    {
        if(request.find("Upgrade") != request.end() && Poco::icompare(request["Upgrade"], "websocket") == 0)
            return new WebsockHandler;
        else
            return new WebHandler;
    }
};

class WebsockServer : public Poco::Util::ServerApplication {
public:
    static int websock_port;
    int main(const vector<string> &args)
    {
        bool quiet=0;
        try {
            StreamSocket tcpsock;
            tcpsock.connect(WebsockHandler::tcpsock_addr);
            tcpsock.close();
            ServerSocket websock(websock_port);
            HTTPServer srv(new RequestHandlerFactory, websock, new HTTPServerParams);
            srv.start();
            quiet=1, close(2);
            waitForTerminationRequest();
        }
        catch(WebSocketException &e) {
            if (quiet) cerr<<"wscatd: "<<e.displayText()<<endl;
        }
        catch(exception &e) {
            if (quiet) cerr<<"wscatd: "<<e.what()<<endl;
        }
        return 1;
    }
} app;

int WebsockServer::websock_port=0;

int main(int argc,char **argv)
{
    int &websock_port=WebsockServer::websock_port;
    int tcpsock_port;
    if (argc==4) {
        WebsockServer::websock_port=atoi(argv[1]);
        tcpsock_port=atoi(argv[3]);
        WebsockHandler::tcpsock_addr=SocketAddress(argv[2],tcpsock_port);
    } else if (argc==3) {
        WebsockServer::websock_port=atoi(argv[1]);
        tcpsock_port=atoi(argv[2]);
        WebsockHandler::tcpsock_addr=SocketAddress("127.0.0.1",tcpsock_port);
    } else
          exit_usage();
    if (websock_port==tcpsock_port
        || websock_port<10 || tcpsock_port<10
        || websock_port>65534 || tcpsock_port>65534) {
        cerr<<"wscatd: invalid port number(s)\n";
        exit_usage();
    }
    if (fork()>0) return 0;
    close(0), close(1);
    return app.run(argc,argv);    
}
