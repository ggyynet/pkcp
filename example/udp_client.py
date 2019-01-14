# coding:utf8
import datetime
import random
import select
import socket
import time
import uuid
from pkcp import Pkcp


class UDPClient:
    def __init__(self, addr):
        self.no = 1
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.connect(addr)
        self.kcp = Pkcp(self.kcp_send, conv=1001, user_data=self, mode=0, max_recv_size=1024)
        print(self.kcp.conv, self.kcp.max_recv_size)

    def kcp_send(self, buffer, user_data):
        self.s.send(buffer)

    def start(self):
        s = self.s
        kcp = self.kcp
        i = 0
        while True:
            timeout = 0.01
            # timeout = min(self.kcp.check() / 1000, 0.01)
            rd, wd, ed = select.select([s], [], [], timeout)
            if rd:
                try:
                    recv_data, addr = s.recvfrom(kcp.max_recv_size)
                except ConnectionRefusedError:
                    break
                if not recv_data:
                    continue
                # 模拟丢包
                if random.randrange(0, 100) < 50:
                    print('drop.')
                    continue
                kcp.input(recv_data)
                data = kcp.recv()
                if data:
                    print(datetime.datetime.now().strftime("%H:%M:%S"),
                          addr, 'size:%d' % len(data), data)
                    args = data.split(' ')
                    no = int(args[-1][3:]) + 1
                    args[-1] = 'no:%d' % no
                    kcp.send(' '.join(args))
            kcp.update()
        s.close()


if __name__ == '__main__':
    address = ('127.0.0.1', 39603)
    uid = str(random.randrange(10000, 99999))
    print('uid: ' + uid)

    c = UDPClient(address)
    send_pack = 'uid:%s time:%s no:%d' % (uid, str(int(time.time())), c.no)
    # print(send_pack)
    c.kcp.send(send_pack)
    c.start()
