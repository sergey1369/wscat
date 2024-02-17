#include <algorithm>
#include <iostream>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/SocketStream.h>
#include <Poco/Net/Socket.h>
#include <Poco/Net/NetException.h>
#include <poll.h>
#include <regex>
#include <string>

using namespace Poco::Net;
using namespace std;

void exit_usage()
{
    cerr<<"Usage: wscat <ws|wss>://host.name[:port]/[full/path]\n";
    exit(2);
}

WebSocket connect(int argc,char *argv[])
{
    if (argc!=2)
        exit_usage();
    string url_pt=    
        "^(wss?)://" // 1 - "ws" or "wss"
        "([a-zA-Z0-9.-]{1,100})" // 2 - hostname
        "(:([0-9]{2,5}))?" // 4 - port num or ""
        "(/[/a-zA-Z0-9.-]{0,100})$"; // 5 - path or ""
    regex url_re(url_pt,regex::extended);
    cmatch m;
    if (!regex_match(argv[1],m,url_re))
        exit_usage();
    HTTPRequest request(HTTPRequest::HTTP_GET, m.str(5), HTTPMessage::HTTP_1_1);
    request.set("User-Agent", "wscat/0.01");
    HTTPResponse response;
    if (m.str(1)=="wss") {
        int port=443;
        if (m.length(4)) port=stoi(m.str(4));        
        HTTPSClientSession cs(m.str(2),port);        
        return WebSocket(cs, request, response);
    } else {
        int port=80;
        if (m.length(4)) port=stoi(m.str(4));
        HTTPClientSession cs(m.str(2),port);
        return WebSocket(cs, request, response);
    }
}

int main(int argc,char *argv[])
{
    try {
        WebSocket ws=connect(argc,argv);
        const int sz0=4096,sz1=32768;
        char data[4096+sz1],*buf=(char *)((1+(size_t)data/4096)*4096);
        struct pollfd fds[2] = { {0,POLLIN,0}, {ws.impl()->sockfd(),POLLIN,0} };
        int result,flags;
        for(;;) {
            while ((result=poll(fds,2,-1))<0 && errno == EINTR);
            if (result < 0) {
                cerr << "wscat: Error in poll()\n";
                break;
            }

            if (fds[0].revents & POLLIN) {
                while ((result=read(0,buf,sz0))<0 && errno == EINTR);
                if (result<0) {
                    cerr << "wscat: Error in read(stdin)\n";
                    break;
                } else if (result>0) 
                    ws.sendFrame(buf, result, WebSocket::FRAME_BINARY);
                else
                    break;
            }

            if (fds[1].revents & POLLIN) {
                result = ws.receiveFrame(buf, sz1, flags);
                if (result>0) {
                    int n;
                    while ((n=write(1,buf,result))<0 && errno == EINTR);
                    if (n<0) {
                        cerr << "wscat: Error in read(stdin)\n";
                        break;
                    }
                    else if (n==0) break;
                    else if (n!=result) {
                        cerr<<"wscat: BUG: Partial write: "<<n<<" of "<<result<<"!!!!\n";
                        break;
                    }
                }
                else if (flags&WebSocket::FRAME_OP_PING)
                    ws.sendFrame(buf, result, WebSocket::FRAME_OP_PONG|WebSocket::FRAME_FLAG_FIN);
                else if (result == 0)
                    break;
                else
                    cerr<<"wscat: receiveFrame problem!\n";

            }
        }

        ws.close();
    }
    catch (const WebSocketException& e) {
        cerr << "wscat: websocket error: "<<e.displayText()<<endl;
    }
    catch (const exception& e) {
        cerr << "wscat: websocket error: "<<e.what()<<endl;
    }
    return 0;
}
