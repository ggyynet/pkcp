# coding:utf8
from typing import Callable, TypeVar

from pkcp._pkcp import pkcp

BS = TypeVar('BS', str, bytes)


class Pkcp:

    def __init__(self, output_callback: Callable[[str, any], None], conv: int = 1001, user_data: any = None,
                 window_size: int = 128,
                 mode: int = 0, max_recv_size: int = 4096):
        """
        :param output_callback:
        :param conv:
        :param user_data:
        :param mode:
        :param window_size:
        """
        p = pkcp(conv, output_callback, udata=user_data)
        p.MAX_RECV_SIZE = max_recv_size
        p.wndsize(window_size, window_size)
        if mode == 0:
            # 默认模式
            p.nodelay(0, 10, 0, 0)
        elif mode == 1:
            # 普通模式，关闭流控等
            p.nodelay(0, 10, 0, 1)
        else:
            # 启动快速模式
            # 第二个参数 nodelay-启用以后若干常规加速将启动
            # 第三个参数 interval为内部处理时钟，默认设置为 10ms
            # 第四个参数 resend为快速重传指标，设置为2
            # 第五个参数 为是否禁用常规流控，这里禁止
            p.nodelay(1, 10, 2, 1)
            p.nodelay(1, 10, 2, 1)
            p.minrto = 10
            p.fastresend = 1
        self._p = p

    def nodelay(self, nodelay: int, interval: int, resend: int, nc: int) -> None:
        self._p.nodelay(nodelay, interval, resend, nc)

    def wndsize(self, sndwnd: int, rcvwnd: int) -> None:
        self._p.wndsize(sndwnd, rcvwnd)

    def input(self, data: BS) -> None:
        return self._p.input(data)

    def update(self) -> None:
        return self._p.update()

    def recv(self) -> BS:
        return self._p.recv()

    def send(self, data: BS) -> None:
        return self._p.send(data)

    def check(self) -> int:
        """
        ikcp_check确定下次调用 update的时间
        :return: 距下次调用的毫秒数
        """
        return self._p.check()

    @property
    def max_recv_size(self):
        return self._p.MAX_RECV_SIZE

    @max_recv_size.setter
    def max_recv_size(self, value):
        self._p.MAX_RECV_SIZE = value

    @property
    def conv(self) -> int:
        return self._p.conv

    @property
    def output_callback(self) -> Callable[[str, any], None]:
        return self._p.output_callback
