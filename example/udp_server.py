# coding:utf8
import datetime
import select
import socket
from pkcp import Pkcp

KCP_MAX_RECV_SIZE = 1024


class Client:

    def __init__(self, addr, kcp_send):
        self.addr = addr
        self.kcp = Pkcp(kcp_send, conv=1001, user_data=self, mode=2, max_recv_size=KCP_MAX_RECV_SIZE)


class UDPServer:
    def __init__(self):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(('127.0.0.1', 39603))
        # self.s.bind(('', 39603))
        self.client_addr = {}

    def kcp_send(self, buffer, user_data):
        self.s.sendto(buffer, user_data.addr)

    def start(self):
        s = self.s
        while True:
            timeout = 0.01
            # timeout = min(self.kcp.check() / 1000, 0.01)
            rd, wd, ed = select.select([s], [], [], timeout)
            if rd:
                try:
                    recv_data, addr = s.recvfrom(KCP_MAX_RECV_SIZE)
                except KeyboardInterrupt:
                    break
                if not recv_data:
                    continue
                if addr not in self.client_addr:
                    self.client_addr[addr] = Client(addr, self.kcp_send)
                kcp = self.client_addr[addr].kcp
                kcp.input(recv_data)
                data = kcp.recv()
                if data:
                    print(datetime.datetime.now().strftime("%H:%M:%S"),
                          addr, 'size:%d' % len(data), data)
                    kcp.send(data)
            self.update()
        s.close()

    def update(self):
        for addr in self.client_addr:
            self.client_addr[addr].kcp.update()
    # s.close()


if __name__ == '__main__':
    s = UDPServer()
    s.start()
