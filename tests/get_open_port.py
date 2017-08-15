#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('', 0))
    port = s.getsockname()[1]
    print(port)
    s.close()


if __name__ == '__main__':
    main()
