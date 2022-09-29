#!/usr/bin/env python

import sys
import socket
import time
import datetime


def get_ydwg_data_time(row):
    elements = row.split(' ')
    dt = datetime.datetime.strptime(elements[0], '%H:%M:%S.%f')
    # kludge to avoid negative or too small timestamps on Windows
    dt2020 = dt.replace(year=2020)
    return dt2020.timestamp()


def serve_data(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        print("Waiting for connection on port {}...".format(port))
        sock.bind(("", port))
        sock.listen()
        conn, addr = sock.accept()
        try:
            with conn:
                print(f"Connected by {addr}")

                while True:
                    data = conn.recv(1024)
                    if not data: break
                    print(data.decode('utf-8'), end="")
        except BrokenPipeError as e:
            print("Client disconnected")


def main():
    port = int(sys.argv[1])

    print("Starting server...")
    serve_data(port)


if __name__ == '__main__':
    main()
