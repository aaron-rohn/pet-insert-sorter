#!/usr/bin/python

import socket, time, datetime, threading, subprocess
import numpy as np

files = ['/mnt/acq/20230217/test/192.168.1.101.SGL',
         '/mnt/acq/20230217/test/192.168.1.102.SGL',
         '/mnt/acq/20230217/test/192.168.1.103.SGL',
         '/mnt/acq/20230217/test/192.168.1.104.SGL']

bytes_per_single = 16
base_port = 10000
buf_size = 1024*1024
lck = threading.Lock()

def worker(i, vals, pause):
    s = socket.create_connection(('127.0.0.1', base_port + i))
    nsingles = 0
    start = datetime.datetime.now()

    with open(files[i], 'rb') as f:
        while True:
            d = f.read(buf_size)
            if len(d) == 0:
                break
            else:
                s.send(d)
                nsingles += len(d) / bytes_per_single
                time.sleep(pause)

    duration = datetime.datetime.now() - start
    s.close()

    with lck:
        vals[i] = (nsingles, duration)

results = open('results.txt', 'w')

for pause in np.logspace(-6, -1, 12):
    print(pause)
    tx_vals = {}
    sorter = subprocess.Popen(
            ['/usr/local/bin/sorter', '/home/aaron/pet-insert-sorter/test/hello.COIN'],
            stdout = subprocess.PIPE, text = True)

    time.sleep(1)

    thr = [threading.Thread(target = worker, args = [i, tx_vals, pause]) for i in range(len(files))]
    [t.start() for t in thr]
    [t.join() for t in thr]

    rx_vals = sorter.stdout.readlines()
    rx_vals = [v.strip().split() for v in rx_vals[-4:]]

    rx_rate = 0
    for n, dur in rx_vals: rx_rate += int(n) / int(dur)

    tx_rate = 0
    for n, dur in tx_vals.values(): tx_rate += n / dur.total_seconds()

    print(f'{pause} {rx_rate} {tx_rate}', flush = True)
    results.write(f'{pause} {rx_rate} {tx_rate}\n')
    results.flush()
    sorter.wait(10)

results.close()
