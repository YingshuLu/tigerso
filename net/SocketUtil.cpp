#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include "net/SocketUtil.h"
#include "core/Logging.h"
#include "core/tigerso.h"
#include "ssl/SSLContext.h"

namespace tigerso {

int SocketUtil::InitSocket(const int domain, const int type, Socket& mcsock) {
	socket_t sockfd = ::socket(domain, type, 0);
    DBG_LOG("socket return: %d", sockfd);
    if(validFd(sockfd)) {
        sockfd = RelocateFileDescriptor(sockfd, MIN_SOCKET_FD);
	    mcsock.setSocket(sockfd);
        return 0;
    }
	return -1;
}

int SocketUtil::Bind(Socket& mcsock, const std::string& s_addr, const std::string& port, const int family) {
	if(!mcsock.exist()) {
		return -1;
	}

	sockaddr_in sock_addr;
	bzero(&sock_addr, sizeof(sock_addr));

	if(PackSockAddr(s_addr, port, family, sock_addr) == -1) {
		return -1;
	}

	mcsock.setStrAddr(s_addr);
	mcsock.setStrPort(port);
	
	socket_t sockfd = mcsock.getSocket();
	
	if(::bind(sockfd, (sockaddr*) &sock_addr, sizeof(sock_addr)) != 0) {
		return -1;
	}
	mcsock.setStage(SOCKET_STAGE_BIND);
	return 0;
}

int SocketUtil::Listen(Socket& mcsock, const int backlog) {
	if(mcsock.getStage() != SOCKET_STAGE_BIND) {
		return -1;
	}

	int back = backlog;
	if(backlog <= 0) {
		back = 5;
	}

    //Dup Listen socket to 64
    /*
    socket_t oldfd = mcsock.getSocket();
    socket_t newfd = ::dup2(mcsock.getSocket(), 64);
    if(newfd != -1) {
        ::close(oldfd);
        mcsock.setSocket(newfd);
    }
    */
    socket_t newfd = RelocateFileDescriptor(mcsock.getSocket(), 64);
    mcsock.setSocket(newfd);

	if(::listen(mcsock.getSocket(), back) != 0) {
		return -1;
	}

    mcsock.listening = true;
	// set socket type and stage 
	mcsock.setRole(SOCKET_ROLE_SERVER);
	mcsock.setStage(SOCKET_STAGE_LISTEN);
	return 0;
}

int SocketUtil::Accept(Socket& listen_mcsock, Socket& accept_mcsock) {
	sockaddr_in client_addr;
	socklen_t addr_len = sizeof(sockaddr_in);
	bzero(&client_addr, sizeof(client_addr));
	
	socket_t client_socket = -1;
	socket_t server_socket = listen_mcsock.getSocket();
	client_socket = ::accept(server_socket, (sockaddr*) &client_addr, &addr_len);
	
	if(client_socket < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) { return 1; }
        DBG_LOG("accept error: %d, reason: %s", errno, strerror(errno));
		return -1;
	}

    client_socket = RelocateFileDescriptor(client_socket, MIN_SOCKET_FD);
	
	std::string s_addr;
	std::string u_port;
	ResolveSockAddr(client_addr, s_addr, u_port);

    accept_mcsock.setSocket(client_socket);
    accept_mcsock.setStrAddr(s_addr);
    accept_mcsock.setStrPort(u_port);
	
	//set socket role and stage
	accept_mcsock.setRole(SOCKET_ROLE_CLIENT);
	accept_mcsock.setStage(SOCKET_STAGE_ACCEPT);
    
    //set tcp connect attribute
    SetKeepAlive(accept_mcsock, true);
    SetTcpNoDelay(accept_mcsock, true);
    SetCloseOnExec(accept_mcsock);
	return 0;
}

int SocketUtil::Connect(Socket& mcsock, const std::string& s_addr, const std::string& port, const int type){
	if(!ValidateAddr(s_addr)|| !ValidatePort(port)) {
        DBG_LOG("ip/port: [%s/%s] invalid", s_addr.c_str(), port.c_str());
		return -1;
	}

	sockaddr_in server_addr;
	socklen_t addr_len = sizeof(sockaddr_in);
	bzero(&server_addr, addr_len);

    if(PackSockAddr(s_addr, port, AF_INET, server_addr) != 0) {
        DBG_LOG("pack str ip to sockaddr failed");
        return -1;
    }

    socket_t sockfd = ::socket(AF_INET, type, 0);
    sockfd = RelocateFileDescriptor(sockfd, 552);

    mcsock.setStrAddr(s_addr);
    mcsock.setStrPort(port);
	mcsock.setRole(SOCKET_ROLE_SERVER); 

    mcsock.setSocket(sockfd);
    mcsock.setNIO(true);
	int ret = ::connect(sockfd, (sockaddr*) &server_addr, addr_len);
    if(ret < 0) {
        //Need recall Connect to establish connection
        if(errno == EINPROGRESS || errno == EALREADY) {
            DBG_LOG("connect need recall: %d, reason: %s", errno, strerror(errno));
	        mcsock.setStage(SOCKET_STAGE_CONNECT);
            return 1;
        }
        DBG_LOG("connect error, errno: %d, reason: %s", errno, strerror(errno));
        mcsock.close();
        return -1;
    }

    DBG_LOG("connection establised immediately");
    SetCloseOnExec(mcsock);
	mcsock.setStage(SOCKET_STAGE_CONNECT);
	return 0;
}

int SocketUtil::Close(Socket& mcsock) {
    if(!mcsock.exist()) {
        return 0;
    }
    INFO_LOG("socket [%d] close now", mcsock.getSocket());
    int ret = ::close(mcsock.getSocket());
    mcsock.setSocket(-1);
    mcsock.setStage(SOCKET_STAGE_CLOSE);
    return 0;
}

int SocketUtil::GraceClose(Socket& mcsock) {
    if(!mcsock.exist()) {
        return 0;
    }
    return shutdown(mcsock.getSocket(), SHUT_WR);
}

//socket util
int SocketUtil::PackSockAddr(const std::string& s_addr, const std::string& port, const int family, sockaddr_in& sock_addr) {
	std::string saddr = s_addr;
	std::string tport = port;
	int fam = family;

	if(s_addr.empty()) {
		saddr = "127.0.0.1";
	}

    if (tport.empty()) {
        tport = "0";
    }

    if(!ValidateAddr(saddr) || !ValidatePort(tport)) {
        DBG_LOG("invalid ip, port > %s:%s", saddr.c_str(), tport.c_str());
        return -1;
    }

	if(family < 0) {
		fam = AF_INET;
	}

	sock_addr.sin_family = fam;
	sock_addr.sin_port = htons(atoi(tport.c_str()));
	
	if(saddr == "127.0.0.1") {
		sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else {
		sock_addr.sin_addr.s_addr = inet_addr(saddr.c_str());
	}

	return 0;
}

int SocketUtil::ResolveSockAddr(const sockaddr_in& sock_addr, std::string& s_addr, std::string& port) {
	char t_addr[256] = {0};
	int u_port = -1;

	socklen_t addr_len = sizeof(t_addr);
	inet_ntop(AF_INET, (void*)&(sock_addr.sin_addr), t_addr, 256);
	s_addr = t_addr;
	u_port = ntohs(sock_addr.sin_port);
	port.assign(std::to_string(u_port));
	return 0;
}

bool SocketUtil::SetAddrReuseable(Socket& mcsock, bool on) {
    if(!mcsock.exist()) {
        return false;
    }

    int yes = on? 1 : 0;
    int ret = setsockopt(mcsock.getSocket(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    return (ret == 0);
}

bool SocketUtil::SetPortReuseable(Socket& mcsock, bool on) {
    if(!mcsock.exist()) {
        return false;
    }
    int yes = on? 1 : 0;
    int ret = setsockopt(mcsock.getSocket(), SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int));
    return ret == 0;
}

bool SocketUtil::SetKeepAlive(Socket& mcsock, bool on) {
    if(!mcsock.exist()) {
        return false;
    }
    int yes = on? 1 : 0;
    int ret = setsockopt(mcsock.getSocket(), SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int));
    return ret == 0;
}

bool SocketUtil::SetTcpNoDelay(Socket& mcsock, bool on) {
    if(!mcsock.exist()) {
        return false;
    }
    int yes = on? 1 : 0;
    int ret = setsockopt(mcsock.getSocket(), IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));
    return ret == 0;
}

bool SocketUtil::SetCloseOnExec(Socket& mcsock) {
    if(!mcsock.exist()) {
        return false;
    }
    
    int flags = fcntl(mcsock.getSocket(), F_GETFD);
    
    flags |= FD_CLOEXEC;

    fcntl(mcsock.getSocket(), F_SETFD, flags);
    return true;
}

bool SocketUtil::ValidateAddr(const std::string& addr) {
	std::regex pattern ("([0-9]{1,3}\\.){3}[0-9]{1,3}");
	return std::regex_match(addr, pattern);
}

bool SocketUtil::ValidatePort(const std::string& sport) {
    int port = atoi(sport.c_str());
	if(port < 0 || port > 65535) {	
		return false;
	}
	return true;
}

int SocketUtil::ResolveHost2IP(const std::string& hostname, std::vector<std::string>& ip_vec ) {
    if (hostname.size() == 0) {
        return -1;
    }
    hostent ent;
    hostent* he_ptr;
    int err;
    char buf[1024] = {0};

    gethostbyname_r(hostname.c_str(), &ent, buf, sizeof(buf), &he_ptr, &err);
    
    char str[256] = {0};
    if (he_ptr != NULL && he_ptr->h_addr_list != NULL) {
        const char **p = (const char**)(he_ptr->h_addr_list);

        while (*p != NULL) {
            
            if (he_ptr->h_addrtype == AF_INET && he_ptr->h_length == sizeof(in_addr)) {
                inet_ntop(he_ptr->h_addrtype, *p, str, sizeof(str));
                ip_vec.push_back(str);
            }
            p = p + 1;
        }
    }
    return 0;
}

bool SocketUtil::TestConnect(Socket& sock) {
    if(!sock.exist()) {
        return false;
    }

    if (sock.getRole() == SOCKET_ROLE_CLIENT) {
        if(sock.getStage() >= SOCKET_STAGE_ACCEPT) {
            return true;
        }
    }
    else if (sock.getRole() == SOCKET_ROLE_SERVER) {
        if(sock.getStage() == SOCKET_STAGE_CONNECT) {
        	sockaddr_in server_addr;
	        socklen_t addr_len = sizeof(sockaddr_in);
	        bzero(&server_addr, addr_len);

            if(PackSockAddr(sock.getStrAddr(), sock.getStrPort(), AF_INET, server_addr) != 0) {
                DBG_LOG("pack str ip to sockaddr failed");
                return false;
            }

	        int ret = ::connect(sock.getSocket(), (sockaddr*) &server_addr, addr_len);
            if(ret == 0 || errno == EISCONN) {
                DBG_LOG("connection established!");
                return true;
            }
            DBG_LOG("test connection result: %d, %s", errno, strerror(errno));
        }
        else if(sock.getStage() == SOCKET_STAGE_RECV || sock.getStage() == SOCKET_STAGE_SEND) {
            return true;
        }
    }
    return false;
}

int SocketUtil::CreateListenSocket(
            const std::string& ipaddr, 
            const std::string& port, 
            const bool unblock,
            Socket& master_sock) {

    std::string ip = ipaddr;
    if (ipaddr.empty() || !ValidateAddr(ipaddr)) {
        ip = "127.0.0.1";
    }

    std::string pt = port;
	if(!ValidatePort(port)) {
		pt = "80";
	}
	
    // socket()
    if (SocketUtil::InitSocket(AF_INET, SOCK_STREAM, master_sock) != 0) {
		DBG_LOG("socket: master socket initilizaion failed");
		return -1;
	}

    // set addr reuse
    if(!SocketUtil::SetAddrReuseable(master_sock, true)) {
        DBG_LOG("error: can not set master socket addr reuseable");
		return 1;
    }

    // set port reuse
    if(!SocketUtil::SetPortReuseable(master_sock, true)) {
        DBG_LOG("error: can not set master socket port reuseable");
		return 2;
    }

    //set noblocking
    master_sock.setNIO(unblock);

    // bind()
	if(SocketUtil::Bind(master_sock, ip, pt, -1) != 0) {
		DBG_LOG("socket: master socket Bind failed");
		return 4;
	}
    
    INFO_LOG("socket[%d] listening on %s:%s", master_sock.getSocket(), ip.c_str(), pt.c_str());

    //listen()
    if(SocketUtil::Listen(master_sock, 50) != 0)
    {
		DBG_LOG("socket: master socket Listen failed");
		return 5;
    }
    return 0;
}

int SocketUtil::CreateUDPConnect(
            const std::string& ipaddr, 
            const std::string& port, 
            const bool unblock,
            Socket& mcsock) {

    std::string ip = ipaddr;
    if (ipaddr.empty() || !ValidateAddr(ipaddr)) {
        return -1;
    }

    std::string pt = port;

    //connect()
    int retcode = 0;
    if((retcode = SocketUtil::Connect(mcsock, ip, port, SOCK_DGRAM)) < 0)
    {
		DBG_LOG("socket: UDP socket connect failed!");
		return -1;
    }

    // set addr reuse
    if(!SocketUtil::SetAddrReuseable(mcsock, true)) {
        DBG_LOG("error: can not set master socket addr reuseable");
		return -1;
    }

    // set port reuse
    if(!SocketUtil::SetPortReuseable(mcsock, true)) {
        DBG_LOG("error: can not set master socket port reuseable");
		return -1;
    }

    mcsock.setNIO(unblock);
	DBG_LOG("UDP socket [%d],  connect to remote server: %s:%s", mcsock.getSocket(), ip.c_str(), port.c_str());

    return retcode;
}

int SocketUtil::RelocateFileDescriptor(int oldfd, int leastfd) {
    if(leastfd < oldfd) {
        return oldfd;
    }

    int newfd = ::fcntl(oldfd, F_DUPFD, leastfd);
    if(0 > newfd) {
        return oldfd;
    }
    ::close(oldfd);
    return newfd;
}

int SocketUtil::Recv(Socket& mcsock, void* buf, size_t len, size_t* recvn) {
    int ret = 0;
    /*original socket*/
    if(!mcsock.isSSL()) {
        ret = ::recv(mcsock.getSocket(), buf, len, MSG_DONTWAIT);
        if(0 >= ret) {
           if( EAGAIN == errno || EWOULDBLOCK == errno) { 
                if(recvn != NULL) {
                    *recvn = ret;
                }
               return TIGERSO_IO_RECALL;
           }
           INFO_LOG("Socket recv error: %d, %s", errno, strerror(errno));
           return TIGERSO_IO_ERROR;
        }
        if(recvn != NULL) { *recvn = ret; }
        return TIGERSO_IO_OK;
    }

    size_t rn = 0;
    ret = mcsock.sctx.recv(buf, len, &rn);
    if(SCTX_IO_ERROR == ret) {
       return TIGERSO_IO_ERROR;
    }
    else if(SCTX_IO_RECALL == ret) {
        if(recvn != NULL) {
            *recvn = rn;
        }
        return TIGERSO_IO_RECALL;
    }

    if(recvn != NULL) { *recvn = rn; }
    return TIGERSO_IO_OK;
}

int SocketUtil::Send(Socket& mcsock, const void* buf, size_t len, size_t* sendn) {
    int ret = 0;
    /*original socket*/
    if(!mcsock.isSSL()) {
        ret = ::send(mcsock.getSocket(), buf, len, MSG_DONTWAIT|MSG_NOSIGNAL);
        if(0 > ret) {
           if( EAGAIN == errno || EWOULDBLOCK) { 
                if(sendn != NULL) {
                    //*sendn = ret;
                    *sendn = 0;
                }
               return TIGERSO_IO_RECALL;
           }
           INFO_LOG("Socket send error: %d, %s", errno, strerror(errno));
           return TIGERSO_IO_ERROR;
        }
        if(sendn != NULL) {
            *sendn = ret;
        }
        return TIGERSO_IO_OK;
    }

    size_t sn = 0;
    ret = mcsock.sctx.send(buf, len, &sn);
    if(SCTX_IO_ERROR == ret) {
       return TIGERSO_IO_ERROR;
    }
    else if(SCTX_IO_RECALL == ret) {
        if(sendn != NULL) {
            *sendn = sn;
        }
        return TIGERSO_IO_RECALL;
    }

    if(sendn != NULL) {
        *sendn = sn;
    }
    return TIGERSO_IO_OK;
}

} //namespace tigerso::net
