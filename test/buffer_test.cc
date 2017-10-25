#include <iostream>
#include <string>
#include "McBuffer.h"
#include "McSocketUtil.h"
#include "McSocket.h"
#include "McSocketPool.h"
#include "mcutil.h"
#include "McDefines.h"

using namespace std;

using namespace mcutil;

#define MCBUFFER_DEBUG_

McSocketPool sock_pool;

void printInfo() {
    sock_pool.info();
}


McSocket listen_sock;

int main(){

    int ret = McSocketUtil::create_listen_socket("10.64.75.131", 1111, false, listen_sock);
    if(ret != 0) {
        if(!listen_sock.is_socket_created()) {
            cout<< "listen socket create failed" << endl;
        }
        cout<< "listen socket create failed:"<< listen_sock.get_socket() <<endl;
        return -1;
    }

    cout<< "listen socket created: "<< listen_sock.get_socket() <<endl;
    int epfd = McSocketUtil::Init_epoll(1024);

    McSocketUtil::Add_epoll_event(epfd, EPOLLIN | EPOLLET, listen_sock);

    printInfo();
    epoll_event events[1024];
    while(true)
    {
        //before wait event, should compute if need to get accpt mutex
        // computeLoadFactor();
        int num = McSocketUtil::Wait_epoll_event(epfd, events, 1024, 10000);
        
        cout<< "epoll wait "<< num << " sockets" <<endl;
        if (num < 0) {
            cout<< errno <<": " <<strerror(errno)<<endl;
            return -1;
        }

        for (int i = 0; i<num; i++) {
            
            cout<< "processing: " <<events[i].data.fd << endl;
            if(events[i].data.fd == listen_sock.get_socket()) {
                cout<< "accepting" <<endl;

                McSocketPtr client_sock = sock_pool.make();
                McSocketUtil::Accept(listen_sock, *client_sock);        

                if(sock_pool.contains(*client_sock)) {
                    printInfo();
                    cout<<"sock pool contain client socket"<<endl;
                }

                int sockfd = client_sock->get_socket();
                if(client_sock->is_socket_created()) {
                    cout<< ">> client ["<<sockfd<<"] accepted" <<endl;
                    cout << ">> recv from:" << client_sock->get_str_addr()<<":"<<client_sock->get_uint_port() <<endl;
                }
                client_sock->set_blocked_IO(false);
                McSocketUtil::Add_epoll_event(epfd, EPOLLIN|EPOLLET, *client_sock);
            }

            if((events[i].events & EPOLLIN) && events[i].data.fd != listen_sock.get_socket()) {
                cout<< "read event: socket "<< events[i].data.fd<<endl;
                MCSOCKET_PTR client_sock = sock_pool.take(events[i].data.fd);

                ssize_t len = client_sock->recvNio();
                if(len == mcutil::MC_SOCKET_IOSTATE_CLOSED || len == mcutil::MC_SOCKET_IOSTATE_ERROR) {
                    McSocketUtil::Del_epoll_event(epfd, *client_sock);
                    client_sock->close();
                    return 0;
                }
                McSocketUtil::Mod_epoll_event(epfd, EPOLLOUT| EPOLLET, *client_sock);

                cout<< "recv " << len << " bytes" <<endl;
                cout<<"client says: " << client_sock->m_buffer.in.toString() <<endl;

            }

            if((events[i].events & EPOLLOUT) && events[i].data.fd != listen_sock.get_socket()) {

                cout<< "write event: socket "<< events[i].data.fd<<endl;
                McSocketPtr client_sock = sock_pool.take(events[i].data.fd);
                ssize_t len = client_sock->sendNio();
                if(len == MC_SOCKET_IOSTATE_CLOSED || len == MC_SOCKET_IOSTATE_ERROR) {
                    
                    McSocketUtil::Del_epoll_event(epfd, *client_sock);
                    client_sock->close();
                    return 0;
                }
                McSocketUtil::Mod_epoll_event(epfd, EPOLLIN| EPOLLET, *client_sock);
            }
        }
    }
    
    printInfo();

    listen_sock.close();
    cout<< "listen closed" <<endl;
 
    return 0;
}

