#!/usr/bin/python
import sys
import socket

class client:
    def __init__(self, addr, port):
        self.addr = addr
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect()

    def connect(self):
        self.sock.connect((self.addr, self.port))
#        self.sock.setblocking(0)

    def send(self, message):
        if len(message):
            self.sock.send(message)

    def recv(self):
        buf = self.sock.recv(40960)
#        if buf:
#            print "Socket recv:", buf
        return buf

    def __del__(self):
        self.sock.close()

def getIPPort(param):
    pal = param.split(':')
    if len(param) < 2:
        #print "IP:port"
        sys.exit(-1)
    return pal[0], int(pal[1])

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print "client.py IP:Port"
    ip,port = getIPPort(sys.argv[1])
    
    filename = sys.argv[2]

    print ip, port
    cli = client(ip, port)
    file = open(filename, 'w')

    cnt = 0;
    empty = False;
    while  True:
        buf = cli.recv()
        if buf:
            file.write(buf)
        else:
            if cnt == 0:
                cnt = cnt + 1

            if not empty:
                cnt = cnt + 1

            if cnt >= 15:
                break;
    file.close()
