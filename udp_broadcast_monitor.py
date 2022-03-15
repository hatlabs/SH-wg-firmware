#!/usr/bin/env python

import sys
import socket

def main():
    port = int(sys.argv[1])
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    # Enable broadcasting mode
    client.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    client.bind(('', port))

    while True:
        data, addr = client.recvfrom(1024)
        print(data.decode('utf-8').strip())


if __name__ == '__main__':
    main()

