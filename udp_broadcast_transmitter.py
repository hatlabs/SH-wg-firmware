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


def send_data(data, ip_addrs, port):
    # FIXME: opening the socket for each line is terribly inefficient
    for ip_addr in ip_addrs:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)  # UDP
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.sendto(data, ("255.255.255.255", port))
        sock.close()


def transmit_data(data, port):
    # kludge to get only the IP address associated with the default route
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    all_ip_addrs = [s.getsockname()[0]]

    #interfaces = socket.getaddrinfo(host=socket.gethostname(), port=None, family=socket.AF_INET)
    #all_ip_addrs = [ip[-1][0] for ip in interfaces]

    current_timestamp = datetime.datetime.now().timestamp()
    reference_timestamp = get_ydwg_data_time(data[0])
    time_offset = current_timestamp - reference_timestamp
    for line in data:
        line_timestamp = get_ydwg_data_time(line)
        now = datetime.datetime.now().timestamp()
        wait_time = line_timestamp - now + time_offset
        if wait_time > 0:
            time.sleep(wait_time)
        send_data((line + "\r\n").encode('utf-8'), all_ip_addrs, port)


def main():
    port = int(sys.argv[1])
    input_file = sys.argv[2]

    with open(input_file) as f:
        input_data = [r.rstrip() for r in f]  # remove line breaks

    print("Transmitting data...")
    transmit_data(input_data, port)


if __name__ == '__main__':
    main()
