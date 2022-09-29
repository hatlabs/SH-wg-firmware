#!/usr/bin/env python

import sys
import socket


def main():
    ip_addr = ""

    if sys.platform == "win32":
        # Windows requires an IP address to bind to
        interfaces = socket.getaddrinfo(
            host=socket.gethostname(), port=None, family=socket.AF_INET
        )
        all_ip_addrs = [ip[-1][0] for ip in interfaces]
        # just get the IP for the first available interface
        ip_addr = all_ip_addrs[0]

    port = int(sys.argv[1])
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    # Enable broadcasting mode
    client.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    client.bind((ip_addr, port))

    while True:
        #print("--")
        data, addr = client.recvfrom(1500)
        print(data.decode("utf-8").strip())


if __name__ == "__main__":
    main()
