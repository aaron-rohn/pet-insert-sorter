import socket, time

files = ['192.168.1.101.SGL', '192.168.1.102.SGL',
         '192.168.1.103.SGL', '192.168.1.104.SGL']

base_port = 20000

socks = [socket.socket(socket.AF_INET, socket.SOCK_STREAM) for _ in files]
[s.connect(('127.0.0.1', base_port + i)) for i,s in enumerate(socks)]

fils = [open(f, 'rb') for f in files]

buf_size = 1024*1024

time.sleep(1)

good = True
while(good):
    for f,s in zip(fils, socks):
        data = f.read(buf_size)
        if len(data) == 0:
            good = False
            break
        s.send(data)
    time.sleep(0.1)

[s.close() for s in socks]
