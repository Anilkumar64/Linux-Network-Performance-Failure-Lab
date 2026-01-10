import socket, struct, time, random

s = socket.create_connection(("127.0.0.1", 9090))
msg = b"PING"
hdr = struct.pack("!I", len(msg))

for b in hdr:
    s.send(bytes([b]))
    time.sleep(random.uniform(0.01, 0.2))

for b in msg:
    s.send(bytes([b]))
    time.sleep(random.uniform(0.01, 0.2))

data = s.recv(1024)
print(data)
s.close()
