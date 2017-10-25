#ifndef TS_NET_SOCKETUTIL_H_
#define TS_NET_SOCKETUTIL_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <stdarg.h>
#include <string>
#include <signal.h>
#include <vector>
#include "net/Socket.h"

namespace tigerso::net {

class SocketUtil {
public:
	//basic socket API
	static int InitSocket(const int domain, const int type, Socket& mcsock);
	static int Bind(Socket& mcsock, const std::string& s_addr, const std::string& port, const int family);
	static int Listen(Socket& listen_mcsock, const int backlog);
	static int Accept(Socket& listen_mcsock, Socket& accept_mcsock);
	static int Connect(Socket& mcsock, const std::string& s_addr, const std::string& port);
    static int Close(Socket&);
    static int GraceClose(Socket&);

	//socket util
	static int ResolveSockAddr(const sockaddr_in&, std::string& s_addr, std::string& port);
	static int PackSockAddr(const std::string& s_addr, const std::string& port, const int family, sockaddr_in& sock_addr);
    static int ResolveHost2IP(const std::string& hostname, std::vector<std::string>& ipVec);
    static bool SetAddrReuseable(Socket&, bool);
    static bool SetPortReuseable(Socket&, bool);
    static bool SetKeepAlive(Socket&, bool);
    static bool SetTcpNoDelay(Socket&, bool);
    static bool SetCloseOnExec(Socket&);
    static bool TestConnect(Socket&);
    static int CreateListenSocket(const std::string& ip, const std::string& port, const bool unblock, Socket& mcsock);
	static bool ValidateAddr(const std::string& addr);
	static bool ValidatePort(const std::string& port);

private:
	SocketUtil();
	SocketUtil(const SocketUtil&);
};

}// namespace tigerso::net

#endif // TS_NET_SOCKETUTIL_H_
