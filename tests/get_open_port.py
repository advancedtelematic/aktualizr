#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    print(port)
    s.close()


if __name__ == '__main__':
    main()
