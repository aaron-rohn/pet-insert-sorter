#!/usr/bin/python

import socket, time, datetime, threading, subprocess
import numpy as np

files = ['/mnt/acq/20230217/test/192.168.1.101.SGL',
         '/mnt/acq/20230217/test/192.168.1.102.SGL',
         '/mnt/acq/20230217/test/192.168.1.103.SGL',
         '/mnt/acq/20230217/test/192.168.1.104.SGL']

n = len(files)
bytes_per_single = 16
base_port = 10000
buf_size = 1024*1024

def worker(i, pause = 4e-3):
    s = socket.create_connection(('127.0.0.1', base_port + i))
    with open(files[i], 'rb') as f:
        while True:
            d = f.read(buf_size)
            if len(d) == 0:
                break
            else:
                s.sendall(d)
                time.sleep(pause)

    s.close()

thr = [threading.Thread(target = worker, args = [i]) for i in range(n)]
[t.start() for t in thr]
[t.join() for t in thr]
