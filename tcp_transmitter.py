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


def transmit_data(data, address, port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        print("Attempting to connect to {}:{}...".format(address, port))
        sock.connect((address, port))
        print("Connected")
        current_timestamp = datetime.datetime.now().timestamp()
        reference_timestamp = get_ydwg_data_time(data[0])
        time_offset = current_timestamp - reference_timestamp
        for line in data:
            line_timestamp = get_ydwg_data_time(line)
            now = datetime.datetime.now().timestamp()
            wait_time = line_timestamp - now + time_offset
            if wait_time > 0:
                time.sleep(wait_time)
            bin_line = (line + "\r\n").encode('utf-8')
            sock.sendall(bin_line)
            #recv_data = sock.recv(1024)
            #print("Received: {}".format(recv_data.decode('utf-8')))


def main():
    address = sys.argv[1]
    port = int(sys.argv[2])
    input_file = sys.argv[3]

    with open(input_file) as f:
        input_data = [r.rstrip() for r in f]  # remove line breaks

    print("Transmitting data...")
    transmit_data(input_data, address, port)


if __name__ == '__main__':
    main()
