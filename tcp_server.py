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


def serve_data(data, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        print("Waiting for connection on port {}...".format(port))
        sock.bind(("", port))
        sock.listen()
        conn, addr = sock.accept()
        try:
            with conn:
                print(f"Connected by {addr}")

                current_timestamp = datetime.datetime.now().timestamp()
                reference_timestamp = get_ydwg_data_time(data[0])
                time_offset = current_timestamp - reference_timestamp
                print("Transmitting data...")
                for line in data:
                    line_timestamp = get_ydwg_data_time(line)
                    now = datetime.datetime.now().timestamp()
                    wait_time = line_timestamp - now + time_offset
                    if wait_time > 0:
                        time.sleep(wait_time)
                    bin_line = (line + "\r\n").encode('utf-8')
                    conn.sendall(bin_line)
                    #recv_data = sock.recv(1024)
                    #print("Received: {}".format(recv_data.decode('utf-8')))
        except BrokenPipeError as e:
            print("Client disconnected")


def main():
    port = int(sys.argv[1])
    input_file = sys.argv[2]

    with open(input_file) as f:
        input_data = [r.rstrip() for r in f]  # remove line breaks

    print("Starting server...")
    serve_data(input_data, port)


if __name__ == '__main__':
    main()
